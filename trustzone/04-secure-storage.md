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
  C. 加密後存入 microSD 的 Secure Storage partition（本專案做法）
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

// 實際 OTP 位址（RM0506 Table 33-36）
#define OTP_ROTPKH_WORD0     152  // OEM_KEY1_ROT0（OTP152-159，8 words = 256 bits）
#define OTP_FW_MIN_VER_WORD  12   // BOOTROM_CONFIG_3 oem_fsbla_monotonic_counter
#define OTP_SECURE_BOOT_WORD 18   // BOOTROM_CONFIG_9 bit[3:0]=secure_boot
```

---

## 加密 Secure Storage（microSD）

Private Key 不能明文存在儲存媒介。設計（板上無 microSD，使用 microSD）：

microSD GPT 分割佈局（Secure Storage 在獨立 partition，加密 Key Blob 存於此）：
```
  ┌──────────────────────────────────────────┐
  │ fsbl1 / fsbl2  BL2 (TF-A)               │ GPT partition
  ├──────────────────────────────────────────┤
  │ fip            BL31 + OP-TEE + U-Boot    │ GPT partition
  ├──────────────────────────────────────────┤
  │ bootfs         FAT32 (kernel + DTB)      │ GPT partition
  ├──────────────────────────────────────────┤
  │ rootfs         ext4 (Linux root)         │ GPT partition
  ├──────────────────────────────────────────┤
  │ secure_store   (optional) Secure Storage │ GPT partition
  │  ├── AES-256 Key Blob (encrypted)        │
  │  ├── ECDSA Private Key Blob              │
  │  └── Replay Protection Block             │
  └──────────────────────────────────────────┘
```

### HUK（Hardware Unique Key）

**HUK 是什麼：** STM32MP21xx 的 HWKEY（Hardware Unique Key）儲存在 OTP376-383，共 8 × 32bit = 256 bits，每顆晶片唯一，出廠時由 ST 燒錄。

> [!warning] HWKEY 不可讀取
> HWKEY（OTP376-383）標記為 **No Access**：CPU 完全無法讀取，HAL API 也不例外。
> HWKEY 透過內部 hardware wire 直接連接到 SAES 加速器，只有 SAES 可以使用。
> 因此 `bsec_get_huk()` 這種「讀取 HUK 再做 HKDF」的模式在 STM32MP21 **不可行**。

STM32MP21 正確的 HUK 使用方式：
```
HWKEY (OTP376-383, No Access, only SAES can use via HW wire)
  └── SAES (CRYP_KEYSEL_HW) --> 直接用 HWKEY 加密，key 值從不進入 CPU
        └── AES-256-GCM encrypt --> Private Key ciphertext
              └── ciphertext stored in microSD secure_store partition
```
用 SAES + `CRYP_KEYSEL_HW` 模式，Private Key 在加密過程中不暴露給軟體。

即使攻擊者讀取了整個 microSD，沒有 HWKEY（焊在晶片內）也無法解密 Private Key。

---

## AES-GCM 加密 Blob 格式

**nonce（Number used ONCE）是什麼：** AES-GCM 每次加密時需要一個隨機的 12-byte 數字，確保即使用同一把 key 加密兩次相同的明文，密文也不同。nonce 必須每次都不同，否則攻擊者可以比較兩次密文推算出 key。

```c
typedef struct __attribute__((packed)) {
    uint8_t  magic[4];       // "SKEY"
    uint32_t version;        // Blob 版本（防 downgrade）
    uint8_t  nonce[12];      // AES-GCM nonce（每次加密用硬體 RNG 隨機產生）
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
    
    // 用 SAES + HWKEY 加密（HUK 不可讀，改用 CRYP_KEYSEL_HW）
    // storage_key 概念保留，實作改用 SAES hardware key 模式
    // 參見 stm32mp2/02-bsec-otp.md HUK 章節
    
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

攻擊者可能把 microSD 的內容換成舊版本（舊 key）：

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
    
    // 3. SAES + HWKEY 解密（HUK No Access，改用 hardware key mode）
    
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

**Provisioning（量產燒錄）是什麼：** 設備在工廠生產時，把必要的金鑰和設定燒進晶片的過程。這個步驟在出貨給客戶之前完成，通常在安全的生產環境（HSM = 硬體安全模組）中執行。

金鑰生命週期分三個階段：生產線 Provisioning → 設備 Runtime → Key Rotation：
```
Provisioning (factory)
  │  1. generate ECDSA key pair (inside HSM)
  │  2. burn HUK to OTP (done at chip factory)
  │  3. burn ROTPKH (Public Key Hash) to OTP
  │  4. encrypt Private Key with HUK, store in microSD
  ▼

Runtime (device operation)
  │  1. M33 reads encrypted key blob from microSD
  │  2. decrypt with HUK (key stays in Secure SRAM, never sent to A35)
  │  3. verify firmware signature with ECDSA private key
  │  4. wipe Secure SRAM after use
  ▼

Key Rotation
  │  1. generate new key pair
  │  2. increment OTP rollback counter
  │  3. encrypt new key with HUK + new counter
  │  4. update microSD
```

---

## 下一步

→ 繼續 [cryptography/](../cryptography/README.md) 了解密碼學基礎
