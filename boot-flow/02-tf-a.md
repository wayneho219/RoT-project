---
tags: [boot-flow, module]
topic: boot-flow
week: "5-6"
---
# Boot Flow Module 2：Trusted Firmware-A（TF-A）

## TF-A 是什麼

**Trusted Firmware-A**（https://www.trustedfirmware.org）是 ARM 官方維護的開源 Secure Boot 和 Secure Monitor 實作。

幾乎所有採用 ARMv8-A 的 SoC（包括 STM32MP2）都以它為基礎。

---

## TF-A 的分層設計

```
BL1  → BootROM 的角色（通常在晶片 ROM，TF-A 可替換）
BL2  → Second Stage Bootloader（Trusted Boot Loader）
BL31 → Runtime Secure Monitor（EL3 常駐）
BL32 → Trusted OS（如 OP-TEE），可選
BL33 → Non-Trusted Firmware（U-Boot / UEFI）
```

---

## BL2：Trusted Boot Loader

### 職責

1. 初始化 DDR（呼叫 ddr_init() / ddr_phy_init()）
2. 從儲存裝置讀取 FIP（Firmware Image Package）
3. 驗證每個 image 的 hash（從 OTP 或憑證鏈取得預期值）
4. 把各 image 複製到正確記憶體位址
5. 把控制權交給 BL31

### BL2 記憶體佈局（STM32MP2 參考）

```
0x0E00_0000  BL2 載入位址（Secure SRAM）
0x0E02_0000  BL2 結束
0x2000_0000  BL31 載入位址
0x2002_0000  OP-TEE 載入位址（可選）
0x8000_0000  U-Boot 載入位址（DDR 起始）
```

### BL2 的 Chain of Trust

```c
// TF-A 的 CoT 透過 X.509 憑證鏈驗證：
//
// ROTPK（Root of Trust Public Key，燒在 OTP 中）
//   └── 驗證 Trusted Key Certificate（由 ROTPK 簽署）
//         └── 驗證 BL31 Content Certificate
//               └── 驗證 BL31 image hash
//         └── 驗證 BL33 Content Certificate
//               └── 驗證 BL33（U-Boot）image hash
```

---

## BL31：Secure Monitor

### 記憶體佈局

BL31 必須一直常駐在 EL3 可存取的記憶體（通常是 Secure SRAM）：

```
BL31 RO: .text, .rodata（唯讀，可執行）
BL31 RW: .data, .bss（可讀寫，不可執行）
BL31 Stack: 每個 CPU 一份 stack
```

### SMC 處理流程

Linux 發出 SMC（如 PSCI cpu_off）後：

```
Linux（EL1-NS）
  │  SMC #0（帶 function ID 和參數）
  ▼
BL31 SMC handler（EL3）
  ├── 讀取 X0（SMC function ID）
  ├── 查 SMC handler table
  │     PSCI_CPU_OFF = 0x8400_0002 → psci_cpu_off()
  │     SIP_SVC_xxx  = 0xC200_xxxx → ST 自訂服務
  │     OPTEE_CALL   = 0x3200_0000 → 轉發到 OP-TEE
  └── ERET 返回 Linux（帶結果）
```

### PSCI（Platform Security Coordination Interface）

```c
// Linux 呼叫 PSCI shutdown
// 底層是 SMC #0, X0 = PSCI_SYSTEM_OFF = 0x8400_0008
system_off() → SMC → BL31 → 關閉所有 CPU → 斷電
```

### SCR_EL3 配置（安全關鍵）

```c
// BL31 初始化時設定 SCR_EL3
val = SCR_RES1_BITS;     // ARMv8-A 要求某些 bit 恆為 1
val |= SCR_NS_BIT;       // 預設 Non-Secure（跑 Linux 時）
val |= SCR_RW_BIT;       // EL1/EL0 使用 AArch64
val |= SCR_FIQ_BIT;      // FIQ 路由到 EL3（Secure）
val |= SCR_HCE_BIT;      // 允許 EL2（Hypervisor）
// SCR_EL3.NS 在切換到 Secure World 時由 BL31 清除
```

---

## OP-TEE（BL32）簡介

**OP-TEE（Open Portable Trusted Execution Environment）**  
是 TrustZone Secure EL1 的 Trusted OS。

```
Non-Secure World          Secure World
─────────────             ─────────────
Linux（EL1-NS）           OP-TEE kernel（EL1-S）
  │  tee-supplicant       │  Trusted Applications（EL0-S）
  │  /dev/tee0            │    ├── Key Storage TA
  │                       │    ├── Crypto TA
  └── ioctl(TEE_IOCTL_OPEN_SESSION)
        │ SMC via BL31
        ▼
      OP-TEE 處理 TA 呼叫
```

本專案 **不一定需要 OP-TEE**（M33 直接做 Secure Storage），  
但如果未來要在 A35 側提供 Trusted Key Storage API，則需要它。

---

## TF-A 原始碼結構

```
trusted-firmware-a/
├── plat/st/stm32mp2/       ← STM32MP2 平台特定程式碼
│   ├── bl2_plat_setup.c    ← BL2 初始化
│   ├── stm32mp2_def.h      ← 記憶體映射定義
│   └── platform.mk         ← 編譯配置
├── bl2/                    ← BL2 通用程式碼
├── bl31/                   ← BL31 通用程式碼
├── services/
│   ├── std_svc/psci/       ← PSCI 實作
│   └── spd/opteed/         ← OP-TEE dispatcher
└── drivers/
    ├── st/                 ← ST 特定驅動（BSEC、NOR flash）
    └── auth/               ← CoT 驗證框架
```

### 編譯 TF-A for STM32MP2

```bash
make PLAT=stm32mp2 \
     ARM_ARCH_MAJOR=8 \
     STM32MP_RAW_FIP=1 \
     BL33=path/to/u-boot.bin \
     all fip
```

---

## 下一步

→ [Module 3：Linux Boot 流程](03-linux-boot.md)
