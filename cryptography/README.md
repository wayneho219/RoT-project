---
tags: [cryptography, index]
topic: cryptography
week: "9"
---
# 密碼學學習筆記

## 本模組聚焦

RoT 專案需要的密碼學，**不是**全面的密碼學教科書，而是：

| 技術 | 用途 |
|------|------|
| SHA-256 | Firmware integrity 驗證 |
| ECDSA-P256 | Firmware 數位簽章 |
| AES-256-GCM | Secure Storage 加密 |
| HKDF | 從 HUK 衍生不同用途的 key |

## 閱讀順序

| 模組 | 主題 |
|------|------|
| [01-hash](01-hash.md) | Hash 函式、SHA-256、integrity check |
| [02-asymmetric](02-asymmetric.md) | 非對稱加密、ECC、ECDSA |
| [03-symmetric](03-symmetric.md) | 對稱加密、AES-GCM、HKDF |
| [04-pki-chain](04-pki-chain.md) | PKI、X.509 憑證鏈、TF-A CoT |
