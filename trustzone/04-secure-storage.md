---
tags: [trustzone, module]
topic: trustzone
week: "5-6"
---
# TrustZone Module 4：Secure Storage 設計

## 問題：Key 要存在哪裡

```
需求：
  1. AES Key 和 ECDSA Private Key 在重開機後仍存在
  2. Normal World（Linux / A35）不能讀取
  3. 防止降版攻擊（Rollback）

選項：
  A. OTP Fuses（BSEC）→ 只能存少量資料（128–256 bytes）
  B. Secure SRAM（揮發性，斷電消失）
  C. 加密後存入 NOR Flash（常見做法）
  D. 外部安全晶片（TPM、SE）→ 最安全，但本專案先不用
```

---

## OTP / BSEC（一次性可程式化記憶體）

BSEC（Boot and Security Controller）管理 STM32MP2 的 OTP fuses：

```c
// 讀取 OTP
uint32_t bsec_read(uint32_t otp_word_idx) {
    // 每個 word 是 32 bits
    // 透過 BSEC IP 暫存器存取
    BSEC->OTPSR = BSEC_START | (otp_word_idx << BSEC_WORD_SHIFT);
    while (BSEC->OTPSR & BSEC_BUSY);          // 等待完成
    return BSEC->OTPDR;                        // 讀取結果
}

// 寫入 OTP（不可逆！）
int bsec_write(uint32_t otp_word_idx, uint32_t value) {
    BSEC->OTPDR = value;
    BSEC->OTPSR = BSEC_PROG | (otp_word_idx << BSEC_WORD_SHIFT);
    while (BSEC->OTPSR & BSEC_BUSY);
    return (BSEC->OTPSR & BSEC_ERROR) ? -1 : 0;
}

// 常用 OTP 佈局（自訂）
#define OTP_ROTPKH_IDX       0   // Root of Trust Public Key Hash（4 words = 128 bits）
#define OTP_FW_MIN_VER_IDX   4   // Firmware 最低版本（anti-rollback counter）
#define OTP_BOOT_LOCK_IDX    5   // Boot source 鎖定 flags
```

---

## 加密 Secure Storage（NOR Flash）

Private Key 不能明文存在 Flash。設計：

```
NOR Flash 佈局：
  ┌─────────────────────────────┐
  │ M33 Secure Firmware（可執行）│ 0x0000_0000
  ├─────────────────────────────┤
  │ A35 Firmware（待驗證）       │ 0x0010_0000
  ├─────────────────────────────┤
  │ Secure Storage               │ 0x0080_0000
  │  ├── AES-256 Key Blob        │
  │  │    └── AES-GCM 加密的 data│
  │  ├── ECDSA Private Key Blob  │
  │  │    └── 用 HUK 加密        │
  │  └── Replay Protection Block │
  │       └── 計數器（防重放攻擊）│
  └─────────────────────────────┘
```

### HUK（Hardware Unique Key）

HUK 是晶片出廠時燒在 OTP 中的唯一金鑰，用來加密其他金鑰：

```
HUK（OTP，對外不可見）
  └── 用 HKDF 衍生 Storage Key
        └── 用 AES-256-GCM 加密 Private Key
              └── 密文存入 NOR Flash
```

即使攻擊者讀取了 NOR Flash，沒有 HUK 也無法解密。

---

## AES-GCM 加密 Blob 格式

```c
typedef struct __attribute__((packed)) {
    uint8_t  magic[4];       // "SKEY"
    uint32_t version;        // Blob 版本（防 downgrade）
    uint8_t  nonce[12];      // AES-GCM nonce（每次加密隨機產生）
    uint32_t ciphertext_len; // 加密資料長度
    uint8_t  ciphertext[];   // AES-GCM 加密的 key 資料
    // 緊接在 ciphertext 後面：
    // uint8_t tag[16];      // AES-GCM authentication tag
} SecureKeyBlob;

// 加密（在 provisioning 階段執行，通常在生產線）
int secure_key_store(const uint8_t *key, uint32_t key_len, 
                     SecureKeyBlob *out) {
    memcpy(out->magic, "SKEY", 4);
    out->version = CURRENT_STORAGE_VERSION;
    
    // 產生隨機 nonce（用硬體 RNG）
    rng_get_bytes(out->nonce, 12);
    
    // 用 HUK 衍生 Storage Key
    uint8_t storage_key[32];
    hkdf_sha256(bsec_get_huk(), 16, "storage", 7, storage_key, 32);
    
    // AES-256-GCM 加密
    out->ciphertext_len = key_len;
    aes_gcm_encrypt(storage_key, out->nonce, 12,
                    key, key_len,
                    out->ciphertext,
                    out->ciphertext + key_len);  // tag 在 ciphertext 後面
    
    // 清除暫時用的 storage_key
    memset(storage_key, 0, sizeof(storage_key));
    return 0;
}
```

---

## Replay Protection（防重放攻擊）

攻擊者可能把 NOR Flash 的內容換成舊版本（舊 key）：

```
攻擊：
  1. 備份 Flash（包含 key blob 和 rollback counter = 1）
  2. 韌體更新（counter = 2，key 更換）
  3. 把 Flash 還原到備份（counter = 1，舊 key）
  → 成功 rollback！

防護：
  把 Rollback Counter 也存在 OTP（不可逆）
  讀取 key blob 時，先驗證 blob 的 counter == OTP 的 counter
  counter 不符 → 拒絕使用
```

```c
int secure_key_load(const SecureKeyBlob *blob, uint8_t *key_out) {
    // 1. 驗證 magic
    if (memcmp(blob->magic, "SKEY", 4) != 0) return -1;
    
    // 2. 防 replay：版本必須 == OTP 記錄的當前版本
    uint32_t expected_ver = bsec_read(OTP_STORAGE_VER_IDX);
    if (blob->version != expected_ver) return -2;
    
    // 3. 衍生 storage key
    uint8_t storage_key[32];
    hkdf_sha256(bsec_get_huk(), 16, "storage", 7, storage_key, 32);
    
    // 4. AES-GCM 解密（同時驗證 tag）
    int ret = aes_gcm_decrypt(storage_key, blob->nonce, 12,
                              blob->ciphertext, blob->ciphertext_len,
                              blob->ciphertext + blob->ciphertext_len,  // tag
                              key_out);
    
    memset(storage_key, 0, sizeof(storage_key));
    return ret;  // tag 驗證失敗 → 返回 -1
}
```

---

## 金鑰生命週期

```
Provisioning（生產線）
  │  1. 產生 ECDSA key pair（在 HSM 中）
  │  2. 燒錄 HUK 到 OTP（晶片出廠時已完成）
  │  3. 燒錄 ROTPKH（Public Key Hash）到 OTP
  │  4. 用 HUK 加密 Private Key，存入 NOR Flash
  ▼

Runtime（設備運行）
  │  1. M33 從 NOR Flash 讀取 encrypted key blob
  │  2. 用 HUK 解密（key 在 Secure SRAM，永不傳給 A35）
  │  3. 用 ECDSA private key 驗證 firmware 簽章
  │  4. 用完後清除 Secure SRAM
  ▼

Key Rotation（金鑰更換）
  │  1. 產生新的 key pair
  │  2. 更新 OTP rollback counter
  │  3. 用新 HUK + 新 counter 加密新 key
  │  4. 更新 NOR Flash
```

---

## 下一步

→ 繼續 [cryptography/](../cryptography/README.md) 了解密碼學基礎
