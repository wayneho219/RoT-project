---
tags: [stm32mp2, module]
topic: stm32mp2
week: "9+"
---
# STM32MP2 Module 1：硬體架構

## SoC 架構

```
STM32MP215F（ARMv8）
│
├── Cortex-A35（AXI Domain）
│     ├── EL3: TF-A BL31（Secure Monitor）
│     ├── EL1-S: OP-TEE（可選）
│     └── EL1-NS: Linux
│
├── Cortex-M33 TD（Trusted Domain，APB Domain）
│     ├── Secure SRAM（M33 私有）
│     ├── BSEC/OTP 直接存取
│     └── RCC control（A35 reset 控制）
│
├── 共享資源
│     ├── NOR Flash（SPI）
│     ├── DDR（LPDDR4，A35 主用，M33 可存取 NS 部分）
│     ├── RCC（Reset and Clock Controller）
│     └── GIC（Generic Interrupt Controller）
│
└── 周邊
      ├── UART（USART1–8）
      ├── SPI（SPI1–6）
      ├── I2C（I2C1–7）
      ├── USB（OTG HS/FS）
      ├── Ethernet（GMAC）
      └── PKA、HASH、RNG（加密加速器）
```

---

## 記憶體映射

```
位址範圍               大小    說明
─────────────────────────────────────────────────────
0x0000_0000            ROM     Boot ROM（BootROM，只讀）
0x0E00_0000  256 KB    SRAM    M33 Secure SRAM（SAU 保護）
0x2000_0000  256 KB    SRAM    NS SRAM（M33/A35 共享通訊）
0x4000_0000            APB1    Peripheral（UART、I2C、SPI...）
0x4400_0000            APB2
0x4800_0000            AHB1    GPIO
0x5000_0000            AHB2    加密加速器（PKA、HASH、RNG）
0x5800_0000            AHB3    BSEC、RCC
0x6000_0000            FMC     Flexible Memory（NOR Flash）
0x8000_0000  1 GB      DDR     LPDDR4（Non-Secure，給 Linux）
0xFE00_0000  32 MB     DDR     Secure DDR（OP-TEE 用，TZASC 保護）
0xE000_0000            PPB     Cortex-M33 系統暫存器（NVIC、MPU、SAU）
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
NOR Flash 32 MB（0x0000_0000 – 0x01FF_FFFF）
  ├── 0x0000_0000 – 0x0001_FFFF  M33 Secure Firmware（128 KB）
  ├── 0x0002_0000 – 0x0002_FFFF  Secure Storage（64 KB）
  │                                （AES 加密的 private key blob）
  ├── 0x0003_0000 – 0x0003_FFFF  Rollback Counter Area（64 KB）
  ├── 0x0004_0000 – 0x00FF_FFFF  A35 Firmware（TF-A FIP）
  │                                （已簽署，等待 M33 驗證）
  └── 0x0100_0000 – 0x01FF_FFFF  空（後續用途）
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
PKA（Public Key Accelerator）
  └── ECDSA 驗章（比純軟體快 ~10x）

HASH
  └── SHA-256/SHA-512/MD5 硬體計算

RNG（True RNG）
  └── 熵源：CMOS 熱雜訊
  └── 輸出：32-bit random words
  └── 符合 NIST SP800-90A

SAES（Secure AES）
  └── AES-128/192/256
  └── 在 Secure Domain 執行，NS 不能存取
  └── Key 可以直接從 HUK 加載（不傳到 CPU）
```

---

## 下一步

→ [Module 2：BSEC/OTP 操作](02-bsec-otp.md)
