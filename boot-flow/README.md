---
tags: [boot-flow, index]
topic: boot-flow
week: "5-6"
---
# Boot Flow 學習筆記

## STM32MP215F-DK 完整開機鏈

```
M33-TD（先）
  └── ROM Code → M33 Secure firmware → 驗證 → release A35

A35（後，被 M33 release）
  └── ROM Code → TF-A BL2 → TF-A BL31 → U-Boot → Linux
```

## 閱讀順序

| 模組 | 主題 |
|------|------|
| [01-boot-chain](01-boot-chain.md) | 完整開機鏈、各階段職責 |
| [02-tf-a](02-tf-a.md) | Trusted Firmware-A（BL1/BL2/BL31/BL33） |
| [03-linux-boot](03-linux-boot.md) | U-Boot → Linux 的 boot 流程 |
| [04-secure-boot](04-secure-boot.md) | Secure Boot 驗證機制、chain of trust |
| [05-linux-internals](05-linux-internals.md) | Linux OS 知識：Process、Syscall、IPC、面試題 |

## 與 RoT 的關係

M33 側的驗證邏輯是整個 boot chain 的起點：沒有 M33 的簽章驗證，A35 就不啟動。  
這樣的設計讓攻擊者即使能修改 NOR Flash 裡的 A35 firmware，也無法讓它執行。
