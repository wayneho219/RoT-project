---
tags: [cryptography, module]
topic: cryptography
week: "9"
---
# 密碼學 Module 4：PKI 與 TF-A 憑證鏈

## PKI（Public Key Infrastructure）

PKI 解決的問題：**怎麼知道一個 public key 是合法的？**

```
問題：
  M33 收到一個 "公鑰"，怎麼確認它真的是合法開發者的公鑰？
  → 如果公鑰本身可以被替換，整個簽章驗證就沒有意義

解法：
  Trust Anchor（信任根）= 燒在 OTP 中的 Root Public Key Hash
  所有其他公鑰都由 Trust Anchor 往下驗證
```

---

## 信任鏈（Chain of Trust）

```
Root CA Key Pair
  └── Self-signed Root Certificate（CA Cert）
        └── 簽發 Intermediate Certificate
              └── 簽發 Leaf Certificate（裝置憑證）
                    └── 用來簽署 firmware

TF-A 的 CoT（Chain of Trust）：

ROTPK（Root of Trust Public Key）
  ├── 燒在 OTP 中（不可修改）
  └── 對應一個 Root Key Pair
        └── 簽發 Trusted Key Certificate（TKCert）
              ├── 包含 BL2 Public Key
              └── 包含 Non-Trusted World Public Key
                    ├── BL2 Public Key
                    │     └── 驗證 BL2 image hash
                    └── Non-Trusted World Public Key
                          └── 驗證 U-Boot image hash
```

---

## X.509 憑證結構

TF-A 使用 X.509 v3 憑證（DER 格式）：

```
Certificate ::= {
    Version:         v3
    SerialNumber:    隨機
    Issuer:          誰簽發的（Root CA DN）
    Validity:        有效期（可以設很遠的未來）
    Subject:         被簽發的對象
    SubjectPublicKey: 這個憑證裡的公鑰
    Extensions:
        TF-A 自訂 OID：
          1.3.6.1.4.1.4128.2100.1 = Hash of Trusted Boot Firmware（BL2）
          1.3.6.1.4.1.4128.2100.3 = Hash of SoC AP Firmware（BL31）
          1.3.6.1.4.1.4128.2100.2 = Non-Trusted Firmware key（BL33 公鑰）
    Signature:       用 Issuer 的 private key 簽
}
```

### 為什麼 TF-A 用憑證而不是直接用 hash

用憑證可以：
1. 分離「誰簽」和「簽了什麼」（不同層次的 key 負責不同層次的 image）
2. 支援 key rotation（換 key 只需要更新憑證鏈）
3. 擴展性強（可以加入更多 policy 欄位）

---

## TF-A CoT 驗證流程

```c
// TF-A BL2 的 authentication framework（auth/auth_mod.c）

// 1. 從 FIP 讀取 Trusted Key Certificate
img_load(TKC_IMG_ID, &tkc_data);

// 2. 用 OTP 中的 ROTPK 驗證 TKCert 的簽章
ret = auth_verify_cert(tkc_data, ROTPK_FROM_OTP);

// 3. 從 TKCert 的 extension 取出 BL2 的 public key
bl2_pubkey = cert_get_extension(tkc_data, BL2_KEY_OID);

// 4. 讀取 BL2 Content Certificate
img_load(BL2CC_IMG_ID, &bl2cc_data);

// 5. 用 bl2_pubkey 驗證 BL2 Content Certificate
ret = auth_verify_cert(bl2cc_data, bl2_pubkey);

// 6. 從 BL2CC 取出 BL2 image 的 hash
bl2_hash = cert_get_extension(bl2cc_data, BL2_HASH_OID);

// 7. 載入 BL2，計算 hash，和 bl2_hash 比較
img_load(BL2_IMG_ID, &bl2_data);
sha256(bl2_data, bl2_size, computed_hash);
if (memcmp_ct(computed_hash, bl2_hash, 32) != 0) panic();
```

---

## 自訂 RoT 的 CoT（本專案）

本專案不使用 TF-A 的 CoT，而是由 M33 直接做簡化版本：

```
ROTPK Hash（OTP）
  └── 驗證 firmware header 裡的 Public Key
        └── 驗證 firmware body 的 ECDSA 簽章

這是 TF-A CoT 的簡化版（只有兩層）
```

---

## 建立測試 CoT（開發用）

```bash
# Step 1: 建立 Root Key Pair（開發用，非生產）
openssl ecparam -name prime256v1 -genkey -noout -out root_key.pem
openssl ec -in root_key.pem -pubout -out root_pubkey.pem

# Step 2: 計算 ROTPKH（要燒進 OTP 的值）
openssl dgst -sha256 -binary root_pubkey.pem > rotpkh.bin
xxd rotpkh.bin  # 看看這 32 bytes 是什麼

# Step 3: 簽署 firmware
python3 tools/sign_firmware.py \
  --firmware build/a35_fw.bin \
  --key root_key.pem \
  --version 1 \
  --output build/a35_fw_signed.bin

# Step 4: 燒錄 ROTPKH 到 STM32MP2 OTP（只在開發板上做）
# 使用 STM32CubeProgrammer
STM32_Programmer_CLI -c port=SWD \
  -otp fuse write 0 $(xxd -p rotpkh.bin | head -c 8)  # Word 0
```

---

## 面試常考：Hash vs HMAC vs 數位簽章

| | Hash（SHA-256）| HMAC | 數位簽章（ECDSA）|
|-|---------------|------|-----------------|
| Key | 無 | 對稱 key | 非對稱 key pair |
| 完整性 | 是（但任何人都能計算）| 是（需要 key）| 是（需要 private key）|
| 不可否認性 | 否 | 否（任何持有 key 的人都能偽造）| 是（只有 private key 持有者）|
| 嵌入式用途 | Integrity check | 快速 MAC | Firmware 驗章 |

---

## 下一步

→ 繼續 [stm32mp2/](../stm32mp2/README.md) 實際硬體整合
