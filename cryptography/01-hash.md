---
tags: [cryptography, module]
topic: cryptography
week: "9"
---
# 密碼學 Module 1：Hash 函式與完整性驗證

## Hash 函式的三個性質

```
1. 確定性：相同輸入 → 永遠相同輸出
2. 單向性：給定 hash，無法還原原始資料（計算上不可行）
3. 碰撞抵抗：找不到兩個不同輸入有相同 hash
```

**用途**：不用傳輸整份資料，只比較 hash，就能確認資料是否一致。

---

## SHA-256

RoT 專案使用 SHA-256（SHA-2 家族的 256-bit 版本）：

```
輸入：任意長度的資料
輸出：固定 32 bytes（256 bits）= 64 個十六進位字元

SHA-256("hello") = 2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824
SHA-256("hello!") = ce06092fb948d9af9e2b3470b4f98a1de3a48a0f5c1f8aa08399f7e5e6ca5f3c
                                                        ↑ 一個字元改變，整個 hash 完全不同
```

### SHA-256 在 RoT 中的用法

```c
// 驗證 firmware 完整性
void verify_firmware_integrity(void) {
    const uint8_t *fw_body = (uint8_t *)A35_FW_BODY_ADDR;
    uint32_t fw_size = firmware_header.image_size;
    
    // 計算實際的 hash
    uint8_t computed[32];
    sha256(fw_body, fw_size, computed);
    
    // 和 header 裡記錄的 hash 比較（constant-time！）
    if (memcmp_ct(computed, firmware_header.sha256, 32) != 0) {
        // firmware 被篡改
        secure_halt();
    }
}
```

---

## SHA-256 計算流程（概念）

```
輸入資料 M（任意長度）
  │
  ▼ 分塊（每塊 512 bits = 64 bytes）
Block 0 | Block 1 | Block 2 | ... | Padding
  │        │        │
  ▼        ▼        ▼
[Compression function（64 rounds）]
  └── 8 個 32-bit 工作變數（A–H）→ 每輪都更新
  └── 最後輸出 256 bits
```

### Padding 規則

SHA-256 把輸入 padding 成 512-bit 的倍數：
- 在末尾加 `0x80`
- 填充 `0x00` 直到還差 8 bytes
- 最後 8 bytes 填原始資料長度（bits）

---

## SHA-256 實作（嵌入式最小化版本）

```c
// SHA-256 context
typedef struct {
    uint32_t state[8];    // H0–H7（256-bit hash 狀態）
    uint64_t bit_count;   // 已處理的 bit 數
    uint8_t  buffer[64];  // 當前未滿 512 bits 的暫存區
    uint32_t buf_len;     // buffer 中的有效 bytes
} SHA256_CTX;

// 使用方式
SHA256_CTX ctx;
sha256_init(&ctx);
sha256_update(&ctx, data_chunk1, len1);   // 可分多次餵入
sha256_update(&ctx, data_chunk2, len2);
sha256_final(&ctx, output);               // 輸出 32 bytes

// 一次性版本（常用）
void sha256(const uint8_t *data, uint32_t len, uint8_t out[32]) {
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, out);
}
```

---

## 硬體加速

STM32MP215F 有硬體 Hash 引擎（HASH peripheral）：

```c
// 使用 ST HAL 的硬體 SHA-256
HAL_HASH_SHA256_Start(&hhash, 
                       (uint8_t *)fw_data, 
                       fw_size, 
                       computed_digest, 
                       HAL_MAX_DELAY);
// 硬體比純軟體快 10–50 倍，且不佔 CPU 時間（可用 DMA）
```

---

## HMAC：帶金鑰的 Hash

單純的 SHA-256 不防止**偽造**（任何人都能計算 hash）。  
HMAC 加入金鑰，只有持有金鑰的人才能產生正確的 MAC：

```c
// HMAC-SHA256
// 輸出：HMAC = SHA256(key XOR opad || SHA256(key XOR ipad || message))

uint8_t mac[32];
hmac_sha256(key, 32, message, msg_len, mac);

// 用途：訊息認證（不用非對稱加密）
// 例如：M33 和 A35 之間的共享記憶體訊息認證
```

---

## 常見誤用

```c
// 錯誤 1：用 == 比較 hash（容易 short-circuit timing attack）
if (memcmp(hash1, hash2, 32) == 0) { ... }  // 改成 memcmp_ct

// 錯誤 2：把 hash 當做加密（hash 是單向，不能加密）
sha256(password, len, hash);                // 儲存密碼的正確方式是 bcrypt/scrypt/argon2
// 在嵌入式 RoT 中不存密碼，但概念要清楚

// 錯誤 3：hash 未覆蓋所有重要欄位（Length Extension Attack）
// SHA-256 不受 length extension 影響，但 SHA-1/MD5 受
// 用 SHA-256 就好，不要用舊算法
```

---

## 下一步

→ [Module 2：非對稱加密與 ECDSA](02-asymmetric.md)
