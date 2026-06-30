---
tags: [stm32mp2, index]
topic: stm32mp2
week: "9+"
---
# STM32MP2 實作筆記

## 開發板：STM32MP215F-DK

```
SoC: STM32MP215F
  ├── Cortex-A35（Application Processor）
  ├── Cortex-M33 TD（Trusted Domain MCU）
  └── 豐富周邊（USB、Ethernet、SPI、I2C、UART...）

板載儲存：
  └── microSD slot（boot source，TF-A FIP + Linux image）

板載 DRAM：
  └── LPDDR4（2 GB）
```

## 閱讀順序

| 模組 | 主題 |
|------|------|
| [01-hardware](01-hardware.md) | 硬體架構、記憶體映射、周邊 |
| [02-bsec-otp](02-bsec-otp.md) | BSEC/OTP 操作、fuse 規劃 |
| [03-m33-td-setup](03-m33-td-setup.md) | M33 TD 初始化、RCC、IPC |
| [04-rot-implementation](04-rot-implementation.md) | 完整 RoT 實作流程 |
| [05-linux-kernel-module](05-linux-kernel-module.md) | Linux Driver：M33 IPC、sysfs、ioctl |
| [06-rtos-on-m33](06-rtos-on-m33.md) | FreeRTOS / Zephyr on M33：Task、Queue、Mutex |
