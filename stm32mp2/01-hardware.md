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
│     ├── NOR Flash (SPI)
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

```
Address Range          Size    Region  Description
─────────────────────────────────────────────────────────────────
0x0000_0000            -       ROM     Boot ROM (read-only)
0x0E00_0000  256 KB    SRAM    M33 Secure SRAM (SAU protected)
0x2000_0000  256 KB    SRAM    NS SRAM (M33/A35 shared IPC)
0x4000_0000            APB1    Peripheral (UART, I2C, SPI...)
0x4400_0000            APB2
0x4800_0000            AHB1    GPIO
0x5000_0000            AHB2    Crypto accelerators (PKA, HASH, RNG)
0x5800_0000            AHB3    BSEC, RCC
0x6000_0000            FMC     Flexible Memory (NOR Flash)
0x8000_0000  1 GB      DDR     LPDDR4 (Non-Secure, Linux)
0xFE00_0000  32 MB     DDR     Secure DDR (OP-TEE, TZASC protected)
0xE000_0000            PPB     Cortex-M33 system regs (NVIC, MPU, SAU)
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
// 相關暫存器
#define RCC_BASE        0x58000000
#define RCC_MP_A35_RST  (RCC_BASE + 0x188)  // A35 reset 控制

// A35 進入 Reset（在 M33 驗證完成前保持）
*((volatile uint32_t *)RCC_MP_A35_RST) = 0x1;   // assert reset

// 驗證通過後釋放 A35 Reset
*((volatile uint32_t *)RCC_MP_A35_RST) = 0x0;   // release reset

// 使用 HAL
__HAL_RCC_A35_FORCE_RESET();     // assert
__HAL_RCC_A35_RELEASE_RESET();   // release
```

---

## NOR Flash（QSPI）

A35 firmware 和 M33 firmware 存在 NOR Flash，透過 SPI/QSPI 存取：

```c
// NOR Flash 規格（STM32MP215F-DK 板載）
// 型號：MX25L256（32 MB，SPI/QSPI，從 0x6000_0000 映射）

// 直接讀（memory-mapped mode）
#define QSPI_BASE  0x60000000
const uint8_t *nor = (const uint8_t *)QSPI_BASE;
uint8_t byte = nor[0x100000];  // 讀取 offset 0x10_0000

// 寫入（需要用 QSPI 指令）
void nor_write_page(uint32_t addr, const uint8_t *data, uint32_t len) {
    // 1. Write Enable（WE）指令
    qspi_send_cmd(NOR_CMD_WEN);
    // 2. Page Program 指令（addr + data）
    qspi_send_cmd_with_addr(NOR_CMD_PP, addr, data, len);
    // 3. 等待 WIP bit 清除
    while (nor_read_status() & NOR_SR_WIP);
}

// Erase Sector（4 KB）
void nor_erase_sector(uint32_t addr) {
    qspi_send_cmd(NOR_CMD_WEN);
    qspi_send_cmd_with_addr(NOR_CMD_SE, addr, NULL, 0);
    while (nor_read_status() & NOR_SR_WIP);
}
```

---

## Flash 佈局規劃

```
NOR Flash 32 MB  (0x0000_0000 - 0x01FF_FFFF)
  ├── 0x0000_0000 - 0x0001_FFFF  M33 Secure Firmware    (128 KB)
  ├── 0x0002_0000 - 0x0002_FFFF  Secure Storage          (64 KB)
  │                                (AES-encrypted private key blob)
  ├── 0x0003_0000 - 0x0003_FFFF  Rollback Counter Area   (64 KB)
  ├── 0x0004_0000 - 0x00FF_FFFF  A35 Firmware (TF-A FIP)
  │                                (signed, pending M33 verification)
  └── 0x0100_0000 - 0x01FF_FFFF  Reserved (future use)
```

---

## ETZPC（Extended TrustZone Protection Controller）

控制每個 peripheral 屬於哪個安全域：

```c
// ETZPC 設定
#define ETZPC_BASE  0x5C007000

typedef enum {
    ETZPC_SECURE   = 0,   // 只有 Secure 可存取
    ETZPC_PRIV_NS  = 1,   // Privileged Non-Secure 可存取
    ETZPC_NS       = 3,   // 任何 Non-Secure 可存取
} ETZPC_Attr;

// 設定 RNG 為 Secure only
etzpc_set_periph_attr(ETZPC_PERIPH_RNG, ETZPC_SECURE);

// 設定 UART 為 Non-Secure
etzpc_set_periph_attr(ETZPC_PERIPH_USART2, ETZPC_NS);
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
