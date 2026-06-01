---
tags: [trustzone, index]
topic: trustzone
week: "5-6"
---
# TrustZone 學習筆記

## 什麼是 TrustZone

ARM TrustZone 是**硬體層級的隔離機制**：在同一顆 CPU 上同時執行「安全」和「不安全」的程式碼，彼此無法互相存取對方的記憶體。

## 兩種 TrustZone

| 版本 | 架構 | 代表核心 |
|------|------|---------|
| TrustZone for A-profile | ARMv8-A | Cortex-A35（A35 側）|
| TrustZone for M-profile | ARMv8-M | Cortex-M33（M33 TD 側）|

概念相似（Secure / Non-Secure 世界），但硬體實作不同。

## 閱讀順序

| 模組 | 主題 |
|------|------|
| [01-overview](01-overview.md) | TrustZone 基本概念、使用場景 |
| [02-tz-a-profile](02-tz-a-profile.md) | A35 的 TrustZone：TZASC、TZMA、SCR_EL3 |
| [03-tz-m-profile](03-tz-m-profile.md) | M33 的 TrustZone：SAU/IDAU、NSC entry |
| [04-secure-storage](04-secure-storage.md) | Secure Storage 設計：金鑰存放、Replay Protection |
