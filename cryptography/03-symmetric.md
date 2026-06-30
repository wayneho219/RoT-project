---
tags: [cryptography, module]
topic: cryptography
week: "9"
---
# 密碼學 Module 3：對稱加密、AES-GCM 與 HKDF

## AES（Advanced Encryption Standard）

```
AES-256：
  Key 大小：256 bits（32 bytes）
  Block 大小：128 bits（16 bytes）
  
  用同一把 key 加密和解密（對稱）
  安全性依賴 key 的保密性
```

---

## AES 的操作模式

**為什麼需要操作模式：** AES 本身設計只加密固定 16 bytes（一個 block）。要加密比 16 bytes 更長的資料，需要定義「怎麼把多個 block 連在一起」，這就是操作模式。

**AEAD（Authenticated Encryption with Associated Data）是什麼：** 加密的同時產生一個「認證標籤（tag）」，解密時先驗 tag，tag 不符就拒絕解密。好處是加密和完整性驗證一次完成，不需要另外加 HMAC。

AES 本身只加密 16 bytes，要加密長資料需要選擇**操作模式**：

| 模式 | 完整性 | 特性 |
|------|--------|------|
| ECB | 無 | 每塊獨立，相同明文 → 相同密文，**不要用** |
| CBC | 無 | 有 IV，但不提供認證 |
| CTR | 無 | 把 AES 當 stream cipher，可並行 |
| **GCM** | **有（AEAD）** | 加密 + 認證，RoT 標準選擇 |
| CCM | 有（AEAD）| 嵌入式常用，比 GCM 稍慢 |

**RoT 使用 AES-256-GCM**（Authenticated Encryption with Associated Data）。

---

## AES-GCM 詳解

GCM = CTR mode 加密 + GHASH 認證

```
輸入：
  key         32 bytes
  nonce（IV） 12 bytes（96-bit，推薦固定長度）
  plaintext   任意長度
  AAD         附加認證資料（不加密但要認證，可選）

輸出：
  ciphertext  和 plaintext 相同長度
  tag         16 bytes（認證標籤）

特性：
  - 任何一個 bit 被修改（ciphertext 或 AAD），tag 驗證就失敗
  - 同一個 (key, nonce) 組合只能用一次！否則安全性崩潰
```

### Nonce 的選擇策略

```c
// 策略 1：隨機產生（需要 TRNG）
rng_get_bytes(nonce, 12);
// 風險：如果 RNG 有問題，可能重複

// 策略 2：計數器（推薦嵌入式）
// 每次加密遞增計數器，存在 microSD secure_store partition（或 OTP）
uint64_t counter = persistent_counter_read();
memcpy(nonce, &counter, 8);       // 前 8 bytes = counter
memset(nonce + 8, 0, 4);          // 後 4 bytes = 0
persistent_counter_increment();   // 燒錄下一個值（OTP）或寫 Flash

// 策略 3：前綴 + 隨機（常見）
// nonce = device_id[4] || random[8]
memcpy(nonce, device_id, 4);
rng_get_bytes(nonce + 4, 8);
```

---

## AES-GCM 程式碼（mbedTLS）

```c
#include "mbedtls/gcm.h"

// 加密（provisioning 時）
int aes_gcm_encrypt(
    const uint8_t *key,       /* 32 bytes */
    const uint8_t *nonce,     /* 12 bytes */
    const uint8_t *plaintext, uint32_t pt_len,
    uint8_t       *ciphertext,
    uint8_t       *tag        /* 16 bytes output */) {

    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);

    mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 256);

    int ret = mbedtls_gcm_crypt_and_tag(
        &ctx,
        MBEDTLS_GCM_ENCRYPT,
        pt_len,
        nonce, 12,
        NULL, 0,             // AAD（不用）
        plaintext,
        ciphertext,
        16, tag);

    mbedtls_gcm_free(&ctx);
    return ret;
}

// 解密（runtime 時）
int aes_gcm_decrypt(
    const uint8_t *key, const uint8_t *nonce,
    const uint8_t *ciphertext, uint32_t ct_len,
    const uint8_t *tag,
    uint8_t       *plaintext) {

    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);
    mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 256);

    // auth_decrypt 同時解密並驗證 tag
    int ret = mbedtls_gcm_auth_decrypt(
        &ctx, ct_len,
        nonce, 12,
        NULL, 0,
        tag, 16,
        ciphertext, plaintext);

    mbedtls_gcm_free(&ctx);
    return ret;  // 0 = 成功，-1 = tag 不符（資料被篡改）
}
```

---

## HKDF（HMAC-based Key Derivation Function）

HKDF 從一個 master secret 衍生多個不同用途的 key：

```
輸入：
  IKM（Input Keying Material）= HUK（32 bytes）
  salt                        = 可選（建議加）
  info                        = 用途說明字串（區分不同衍生的 key）
  len                         = 想要的 output 長度

輸出：
  OKM（Output Keying Material）= 衍生的 key
```

### 為什麼需要 HKDF

```
不好的做法：
  HUK 直接當 AES key → 如果 AES key 洩漏，HUK 也洩漏

好的做法：
  HUK → HKDF("storage-key", 32 bytes) → AES key（加密 private key 用）
  HUK → HKDF("attestation-key", 32 bytes) → 另一個 key（attestation 用）
  
  每個用途有獨立的衍生 key，互相不影響
```

```c
// HKDF-SHA256 衍生不同用途的 key
uint8_t huk[16];
bsec_read_huk(huk);  // 從 OTP 讀取 HUK

// 衍生 Secure Storage 的 AES key
uint8_t storage_key[32];
hkdf_sha256(
    huk, 16,             // IKM
    NULL, 0,             // salt（可選）
    "storage", 7,        // info（區分用途）
    storage_key, 32);    // output

// 衍生 Attestation 用的 key（不同 info → 完全不同的 key）
uint8_t attest_key[32];
hkdf_sha256(
    huk, 16,
    NULL, 0,
    "attestation", 11,
    attest_key, 32);

// 用完立刻清除（不要讓 key 在 memory 裡殘留）
memset(storage_key, 0, sizeof(storage_key));
```

### HKDF 的兩個步驟

```c
// HKDF = Extract + Expand
// 1. Extract：把 IKM 壓縮成固定長度的 PRK（Pseudorandom Key）
//    PRK = HMAC-SHA256(salt, IKM)
//
// 2. Expand：把 PRK 展開成所需長度
//    T(1) = HMAC-SHA256(PRK, info || 0x01)
//    T(2) = HMAC-SHA256(PRK, T(1) || info || 0x02)
//    ...
//    OKM = T(1) || T(2) || ...（取前 len bytes）
```

---

## 隨機數產生（TRNG）

**TRNG vs PRNG 的差別：**

```
PRNG（Pseudo-Random Number Generator，偽隨機）：
  用數學公式從一個 seed 產生一串「看起來很隨機」的數字
  給定相同的 seed → 輸出完全可預測
  攻擊者若知道 seed 或推算出 seed → 破解所有 key

TRNG（True Random Number Generator，真隨機）：
  從物理現象取得熵（entropy）：CPU 熱雜訊、電路噪音
  真正不可預測，即使攻擊者知道演算法也無法預測輸出
  密碼學操作（產生 key、nonce）必須用 TRNG
```

密碼學操作需要真隨機數（TRNG），不能用偽隨機數（PRNG）：

```c
// STM32MP215F 有硬體 TRNG（True Random Number Generator）
// 透過 HAL_RNG 存取

RNG_HandleTypeDef hrng;
hrng.Instance = RNG;
HAL_RNG_Init(&hrng);

// 取 4 bytes 隨機數
uint32_t random_word;
HAL_RNG_GenerateRandomNumber(&hrng, &random_word);

// 取 32 bytes（for AES key）
uint8_t random_key[32];
for (int i = 0; i < 8; i++) {
    uint32_t word;
    HAL_RNG_GenerateRandomNumber(&hrng, &word);
    memcpy(random_key + i * 4, &word, 4);
}
```

---

## 下一步

→ [Module 4：PKI 與 TF-A 憑證鏈](04-pki-chain.md)
