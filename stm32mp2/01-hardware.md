---
tags: [stm32mp2, module]
topic: stm32mp2
week: "9+"
---
# STM32MP2 Module 1：硬體架構

## 開發板規格（STM32MP215F-DK）

**型號**：STM32MP215F-DK（板卡編號 MB2059，晶片 STM32MP215FAN3）

**初次發布**：2025 年 10 月，文件為 Revision 1

### 處理器

| 核心 | 架構 | 速度 | 用途 |
|------|------|------|------|
| Cortex-A35 | ARMv8-A（64-bit） | 最高 1.5 GHz | Linux、應用程式、TF-A |
| Cortex-M33 | ARMv8-M（32-bit） | 300 MHz | RoT、安全啟動、即時控制 |

### 記憶體

| 項目 | 規格 | 換算 |
|------|------|------|
| LPDDR4 DRAM | 16-Gbit | **2 GB**（16 ÷ 8） |

### 板載周邊

```
Connectivity:
  ├── Ethernet RJ45 (100 Mbit/s, RMII)
  ├── USB 2.0 Type-C
  ├── microSD slot
  ├── M.2 E-Key (Wi-Fi / Bluetooth SDIO)
  └── GPIO header (Raspberry Pi shield compatible)

Display / Camera:
  ├── LTDC display connector (LCD)
  └── Dual-lane MIPI CSI-2 camera connector

Debug:
  ├── MIPI10 JTAG
  ├── STDC14 debug connector
  └── ST-LINK external (not onboard, unlike MP257F-DK)

Misc:
  ├── 4x user LEDs
  ├── 2x user buttons, 1x tamper button, 1x reset button
  ├── Wake-up button
  ├── 4x boot pin switches (select boot source)
  └── VBAT backup power
```

### 開發工具（官方支援）

- **Yocto Project** — 建立完整 Linux image（主要開發環境）
- **Buildroot** — 輕量替代方案
- **STM32CubeIDE** — M33 韌體開發

### 型號命名規則

```
STM32MP2 - 15 - F  - DK
  │          │    │    └── Discovery Kit（開發板）
  │          │    └──── F：Secure Boot + 密碼學硬體 + 最高頻率
  │          └──────── 215：產品系列
  └─────────────────── STM32MP2：STM32 Cortex MPU 系列
```

**F 型號代表**：
- Secure Boot 硬體支援
- PKA（ECDSA 加速）、HASH（SHA-256 加速）、RNG、SAES
- 這是做 RoT 必須選 F 型號的原因

---

## SoC 架構

```
STM32MP215F (ARMv8)
│
├── Cortex-A35 (AXI Domain)
│     ├── EL3: TF-A BL31 (Secure Monitor)
│     ├── EL1-S: OP-TEE (optional)
│     └── EL1-NS: Linux
│
├── Cortex-M33 TD (Trusted Domain, APB Domain)
│     ├── Secure SRAM (M33 private)
│     ├── BSEC/OTP direct access
│     └── RCC control (A35 reset)
│
├── Shared Resources
│     ├── DDR (LPDDR4, primary A35, M33 accesses NS region)
│     ├── RCC (Reset and Clock Controller)
│     └── GIC (Generic Interrupt Controller)
│
└── Peripherals
      ├── UART (USART1-8)
      ├── SPI (SPI1-6)
      ├── I2C (I2C1-7)
      ├── USB (OTG HS/FS)
      ├── Ethernet (GMAC)
      └── PKA, HASH, RNG (crypto accelerators)
```

---

## 記憶體映射

STM32MP21 的 peripheral 有兩組別名地址：
- `0x4xxx_xxxx`：非安全別名（A35 NS 或 M33 非安全存取）
- `0x5xxx_xxxx`：安全別名（M33 Secure 存取用，地址差 0x1000_0000）

```
Address Range          Bus      Description
─────────────────────────────────────────────────────────────────
0x0000_0000            ROM      Boot ROM（僅 A35 可見）
0x0A00_0000            SRAM     SYSRAM + RETRAM + SRAM1（非安全別名）
0x0E00_0000            SRAM     SYSRAM + RETRAM + SRAM1（安全別名）
                                 SYSRAM 256 KB / RETRAM 128 KB / SRAM1 64 KB
0x4000_0000            APB1     TIM2, UART, SPI, I2C...
0x4020_0000            APB2     TIM1, USART1, SPI1...
0x4040_0000            AHB2     HPDMA, Cache, ADC, OCTOSPI
0x4200_0000            AHB3     HASH, RNG, SAES, PKA, CRYP, RIFSC
0x4400_0000            APB3     BSEC（0x4400_0000）
0x4420_0000            AHB4     RCC（0x4420_0000）, GPIOA（0x4424_0000）...
0x5000_0000            -        （同 0x4xxx，Secure alias，位址差 0x1000_0000）
0x6000_0000            FMC      External memory space（板上未接 NOR Flash）
0x8000_0000  2 GB      DDR      LPDDR4（Linux / A35 用）
0xE000_0000            PPB      Cortex-M33 system regs（NVIC, MPU, SAU）
```

### 從 C 存取暫存器

```c
// STM32MP2 官方 HAL header 定義了所有暫存器位址
#include "stm32mp215xx.h"

// 例：讀取 RCC（Reset and Clock Controller）
RCC->MP_AHB5RSTCLRR;  // 清除 Cortex-A35 的 reset

// 例：BSEC OTP
BSEC->OTP_DATA[0];   // 讀取 OTP word 0
```

---

## RCC（Reset and Clock Controller）

M33 控制 A35 的 reset，透過 RCC 暫存器：

```c
// RCC base address（來自 RM0506 Table 9）
#define RCC_BASE_NS     0x44200000   // 非安全別名
#define RCC_BASE_S      0x54200000   // 安全別名（M33 Secure 用）

// 具體暫存器 offset 和名稱需從 STM32CubeMP2 header 確認
// 使用 HAL（推薦）
__HAL_RCC_A35_FORCE_RESET();     // assert A35 reset
__HAL_RCC_A35_RELEASE_RESET();   // release A35 reset
```

---

## 開機儲存媒介（microSD）

STM32MP215F-DK 沒有板載 NOR Flash，boot source 是 **microSD**。
Boot pin switches 選擇開機來源（預設 SD card）。

microSD 使用 GPT 分割表，STM32MP2 官方 Yocto 的標準分割佈局：

```
microSD（GPT 分割）
  ├── fsbl1   （TF-A BL2，~256 KB）    First Stage Boot Loader，ROM 載入
  ├── fsbl2   （TF-A BL2 備份）         冗餘備份，啟動失敗時切換
  ├── fip     （FIP image，~4 MB）      BL31 + OP-TEE + U-Boot 打包
  ├── bootfs  （FAT32，~64 MB）         kernel Image + DTB
  └── rootfs  （ext4，剩餘空間）         Linux root filesystem
```

ROM Boot Loader（晶片內建，不可改）讀取 fsbl1/fsbl2 → 跳到 BL2 → 載入 FIP。

---

## RIFSC（Resource Isolation Framework Security Controller）

STM32MP21 用 RIFSC 取代舊版 STM32MP1 的 ETZPC，控制每個 peripheral 的安全屬性：

```c
// RIFSC base address（來自 RM0506 Table 9，Chapter 8）
#define RIFSC_BASE_NS  0x42080000   // 非安全別名
#define RIFSC_BASE_S   0x52080000   // 安全別名

// 安全屬性設定（概念，實際 API 見 STM32CubeMP2 HAL）
// 設定 peripheral 為 Secure only → M33 獨佔
// 設定 peripheral 為 Non-Secure → A35 可存取
```

---

## 硬體加密加速器

```
PKA (Public Key Accelerator)
  └── ECDSA verify (~10x faster than software)

HASH
  └── SHA-256 / SHA-512 / MD5 hardware computation

RNG (True RNG)
  └── Entropy source: CMOS thermal noise
  └── Output: 32-bit random words
  └── Compliant with NIST SP800-90A

SAES (Secure AES)
  └── AES-128 / 192 / 256
  └── Runs in Secure Domain only (NS cannot access)
  └── Key loaded directly from HUK (never exposed to CPU)
```

---

## 下一步

→ [Module 2：BSEC/OTP 操作](02-bsec-otp.md)
