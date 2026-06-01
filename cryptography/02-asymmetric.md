---
tags: [cryptography, module]
topic: cryptography
week: "9"
---
# 密碼學 Module 2：非對稱加密與 ECDSA

## 為什麼需要非對稱加密

```
對稱加密（AES）的問題：
  加密和解密用同一個 key
  要驗證 firmware，設備上必須有這個 key
  → 攻擊者讀出 key 後可以偽造 firmware！

非對稱加密的解法：
  Public Key  → 存在設備上（可以公開）
  Private Key → 只有 firmware 開發者持有（永遠不放上設備）
  
  開發者：用 Private Key 簽署 firmware
  設備：用 Public Key 驗證簽章 → 只有擁有 Private Key 的人才能產生合法簽章
```

---

## ECC（橢圓曲線密碼學）基礎

RSA 和 ECC 都是非對稱密碼，但 ECC 的優勢：

| | RSA-2048 | ECDSA-P256 |
|-|---------|------------|
| 安全等級 | ~112 bits | ~128 bits |
| Key 大小 | 2048 bits | 256 bits（8 倍小）|
| 簽章大小 | 256 bytes | 64 bytes |
| 計算速度 | 較慢 | 較快（嵌入式友善）|

RoT 專案用 **ECDSA-P256**（P-256 = NIST P-256 = secp256r1）。

---

## ECC 的數學直覺（不需深入）

```
橢圓曲線：y² = x³ + ax + b（在有限域上）

Private Key（d）：一個隨機大整數（256 bits）
Public Key（Q）：Q = d × G（G 是曲線的固定基點）

離散對數問題：知道 Q 和 G，求 d 在計算上不可行（這是安全性的基礎）
```

---

## ECDSA 簽章與驗證

### 簽署（在開發機 / HSM 上）

```
輸入：firmware_hash（32 bytes），private_key（d，32 bytes）
輸出：signature（r, s，各 32 bytes = 64 bytes 總長）

步驟（概念）：
  1. 產生隨機數 k
  2. 計算 R = k × G，取 R.x mod n 得到 r
  3. 計算 s = k⁻¹(hash + d×r) mod n
  4. 輸出 (r, s)
```

### 驗證（在 M33 設備上）

```
輸入：firmware_hash，signature(r, s)，public_key(Q)
步驟：
  1. 計算 w = s⁻¹ mod n
  2. 計算 u₁ = hash × w mod n
  3. 計算 u₂ = r × w mod n
  4. 計算 X = u₁ × G + u₂ × Q
  5. 驗證 X.x mod n == r？→ 成立則簽章有效
```

---

## 實際使用（呼叫密碼學函式庫）

嵌入式不需要自己實作 ECDSA，使用 **mbedTLS** 或 **WolfSSL**：

### 開發機：產生金鑰對

```bash
# 用 OpenSSL 產生 P-256 key pair
openssl ecparam -name prime256v1 -genkey -noout -out private_key.pem
openssl ec -in private_key.pem -pubout -out public_key.pem

# 提取 raw bytes（嵌入式用）
openssl ec -in private_key.pem -outform DER | tail -c +8 | head -c 32 > private_key.bin
openssl ec -in public_key.pem  -outform DER | tail -c +28 > public_key.bin  # 65 bytes（04 + x + y）
```

### 開發機：簽署 firmware

```bash
# 計算 firmware hash
openssl dgst -sha256 -binary firmware.bin > firmware.hash

# 用私鑰簽署（輸出 DER 格式的 ECDSA 簽章）
openssl dgst -sha256 -sign private_key.pem firmware.bin > signature.der

# 轉成 raw r||s（64 bytes，嵌入式驗章用）
python3 tools/der_to_raw_sig.py signature.der > signature.bin
```

### M33：驗證（mbedTLS）

```c
#include "mbedtls/ecdsa.h"
#include "mbedtls/sha256.h"

int verify_firmware_signature(
    const uint8_t *fw_body,    uint32_t fw_size,
    const uint8_t *sig_raw,    /* 64 bytes: r(32) || s(32) */
    const uint8_t *pubkey_raw  /* 65 bytes: 04 || x(32) || y(32) */) {

    // 1. 載入 public key
    mbedtls_ecp_group grp;
    mbedtls_ecp_point Q;
    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&Q);
    mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    mbedtls_ecp_point_read_binary(&grp, &Q, pubkey_raw, 65);

    // 2. 把 raw sig 轉成 mbedTLS 格式
    mbedtls_mpi r, s;
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);
    mbedtls_mpi_read_binary(&r, sig_raw,      32);
    mbedtls_mpi_read_binary(&s, sig_raw + 32, 32);

    // 3. 計算 firmware hash
    uint8_t hash[32];
    mbedtls_sha256(fw_body, fw_size, hash, 0);  // 0 = SHA-256（不是 SHA-224）

    // 4. 驗章
    int ret = mbedtls_ecdsa_verify(&grp, hash, 32, &Q, &r, &s);

    // 5. 清理
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&Q);
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);

    return (ret == 0) ? 0 : -1;
}
```

---

## Public Key 的安全存放

Public Key 可以公開，但**需要保護它的完整性**：

```
不安全方式：
  把 public_key.bin 直接 link 進 M33 firmware
  → 攻擊者換掉 firmware，同時換掉 public key

安全方式（推薦）：
  只存 ROTPKH（Root of Trust Public Key Hash = SHA-256(pubkey)）到 OTP
  OTP 不可修改
  M33 每次啟動從 OTP 讀取 ROTPKH，驗證 firmware 裡的 pubkey 和它一致
  再用 pubkey 驗章
```

```c
void secure_boot_verify(void) {
    // 1. 從 OTP 讀取 ROTPKH
    uint8_t rotpkh_otp[32];
    bsec_read_rotpkh(rotpkh_otp);

    // 2. 從 firmware header 讀取 pubkey
    const uint8_t *pubkey = firmware_header.pubkey;  // 65 bytes

    // 3. 計算 pubkey 的 hash
    uint8_t pubkey_hash[32];
    sha256(pubkey, 65, pubkey_hash);

    // 4. 驗證和 OTP 的一致性
    if (memcmp_ct(pubkey_hash, rotpkh_otp, 32) != 0) {
        secure_halt();  // pubkey 被篡改！
    }

    // 5. 用這個 pubkey 驗章（接下來才是真正的 ECDSA）
    if (verify_firmware_signature(fw_body, fw_size, sig, pubkey) != 0) {
        secure_halt();
    }
}
```

---

## 硬體加速（PKA）

STM32MP215F 有 PKA（Public Key Accelerator）硬體：

```c
// 使用 ST HAL PKA 的 ECDSA 驗證
PKA_ECDSAVerifInTypeDef in = {
    .primeOrderSize = 32,          // P-256 的 n
    .modulusSize    = 32,          // P-256 的 p
    .coefSign       = 1,           // a 是負數（P-256 a = -3）
    .coef           = p256_a,
    .modulus        = p256_p,
    .primeOrder     = p256_n,
    .pPubKeyCurvePtX = pubkey_x,
    .pPubKeyCurvePtY = pubkey_y,
    .RSign           = sig_r,
    .SSign           = sig_s,
    .hash            = firmware_hash,
};

HAL_PKA_ECDSAVerif(&hpka, &in, HAL_MAX_DELAY);
uint32_t result = HAL_PKA_ECDSAVerif_IsValidSignature(&hpka);
// result == 1 → 簽章有效
```

---

## 下一步

→ [Module 3：對稱加密與 HKDF](03-symmetric.md)
