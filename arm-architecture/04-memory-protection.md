---
tags: [arm-architecture, module]
topic: arm-architecture
week: "3-4"
---
# ARM 架構 Module 4：記憶體保護（MMU / MPU）

## 兩種機制的對比

| 機制 | 架構 | 概念 |
|------|------|------|
| **MMU**（Memory Management Unit）| ARMv8-A（A35）| 虛擬位址 → 實體位址，頁表分頁 |
| **MPU**（Memory Protection Unit）| ARMv8-M（M33）| 實體位址區域的存取權限 |

A35 用 MMU 給 Linux 做虛擬記憶體；M33 用 MPU 做基本存取保護。

---

## MMU（A35 側）

### 虛擬位址映射

Linux 在 A35 上執行時，每個 process 都有自己的虛擬位址空間：

```
虛擬位址空間（64-bit user space）
  0x0000_0000_0000_0000 – 0x0000_FFFF_FFFF_FFFF  → User space
  0xFFFF_0000_0000_0000 – 0xFFFF_FFFF_FFFF_FFFF  → Kernel space

實體記憶體（STM32MP215F-DK）
  0x0000_0000 – 0x3FFF_FFFF  → DDR（1 GB）
  0x0E00_0000 – 0x0EFF_FFFF  → Secure SRAM（M33 用）
  0x0800_0000 – 0x0BFF_FFFF  → NOR Flash
```

### 頁表（Page Table）

```
頁大小：4 KB（0x1000）
最小映射單位：一個 page

VA 0x400000 → PA 0x40001000  （可讀可執行，non-cacheable）
VA 0x1000   → 無映射         → Page Fault
```

Boot 期間，TF-A 和 U-Boot 用 **恆等映射**（identity mapping）：  
`VA == PA`，簡化程式碼。Linux 啟動後才建立完整頁表。

### TLB（Translation Lookaside Buffer）

頁表查詢很慢，TLB 是快取：

```c
// 切換 process 時，Linux 會 flush TLB
// TF-A 切換世界前也要做
asm("tlbi vmalle1");    // 讓 EL1 的所有 TLB 失效
asm("dsb sy");          // Data Synchronization Barrier
asm("isb");             // Instruction Synchronization Barrier
```

### Memory Attributes（MAIR_EL1）

```
Device-nGnRnE  → 硬體暫存器（MMIO），不可 cache，不可 reorder
Normal         → 一般記憶體（DDR），可以 cache
Normal NC      → Non-Cacheable DDR（共享記憶體用）
```

---

## MPU（M33 側）

M33 MPU 最多 **16 個 region**，每個 region 獨立設定存取權限：

### Region 屬性

```c
// 每個 region 設定：
// - 起始位址（必須對齊到 region 大小）
// - 結束位址
// - 屬性（可讀、可寫、可執行、Secure/NS）

typedef struct {
    uint32_t base;       // 對齊要求：至少 32 bytes 對齊
    uint32_t limit;
    uint8_t  attr_idx;   // 指向 MAIR 中的 memory attribute
    uint8_t  sh;         // Shareable（多核心共用記憶體時需要）
    uint8_t  ap;         // Access Permission（read-only / read-write）
    uint8_t  xn;         // Execute Never（資料段不可執行）
} MPU_Region;
```

### STM32MP215F 的 MPU 配置範例

```c
// RoT 專案的 MPU 規劃：
//
// Region 0: Secure Flash（可執行，唯讀）
//   0x0E000000 – 0x0E07FFFF（512 KB）
//   AP = RO, XN = 0, Secure

// Region 1: Secure SRAM（可讀寫，不可執行）
//   0x20000000 – 0x2001FFFF（128 KB）
//   AP = RW, XN = 1, Secure

// Region 2: Non-Secure SRAM（給 A35 共享）
//   0x20020000 – 0x2003FFFF（128 KB）
//   AP = RW, XN = 1, Non-Secure

// Region 3: M33 Peripheral（NVIC、SysTick 等）
//   0xE0000000 – 0xE00FFFFF
//   Device memory, XN = 1

void mpu_init(void) {
    // 先停用 MPU
    MPU->CTRL = 0;

    // Region 0: Secure Code Flash
    MPU->RNR  = 0;
    MPU->RBAR = 0x0E000000 | MPU_RBAR_SH_NON | MPU_RBAR_AP_RO | MPU_RBAR_XN_Msk;
    MPU->RLAR = (0x0E07FFFF & MPU_RLAR_LIMIT_Msk) | MPU_RLAR_EN_Msk;

    // Region 1: Secure SRAM
    MPU->RNR  = 1;
    MPU->RBAR = 0x20000000 | MPU_RBAR_SH_NON | MPU_RBAR_AP_RW | MPU_RBAR_XN_Msk;
    MPU->RLAR = (0x2001FFFF & MPU_RLAR_LIMIT_Msk) | MPU_RLAR_EN_Msk;

    // 啟用 MPU，背景 region 用預設 privileged access
    MPU->CTRL = MPU_CTRL_ENABLE_Msk | MPU_CTRL_PRIVDEFENA_Msk;

    __DSB();
    __ISB();
}
```

---

## Secure / Non-Secure 記憶體隔離

M33 TrustZone + MPU 的雙重保護：

```
Secure Code Flash（SAU region, Secure）
  ├── Non-Secure 程式碼嘗試讀取 → SecureFault
  └── MPU 設為 RO → Secure 程式碼自己也不能意外寫入

Secure SRAM（SAU region, Secure）
  ├── Non-Secure 嘗試存取 → SecureFault
  └── 存放：私鑰、驗證中的 firmware digest

Non-Secure SRAM（SAU region, Non-Secure）
  ├── M33 Secure code 可以存取（Secure 可以看 NS 記憶體）
  └── A35 可以存取（透過 AXI 匯流排）
  
共享記憶體設計原則：
  - A35 只能寫入 NS SRAM，M33 負責讀取並驗證
  - M33 向 A35 回傳結果也透過 NS SRAM
  - 驗證用的中間資料（hash、key）永遠留在 S SRAM
```

---

## 記憶體屏障（Memory Barriers）

ARM 架構是弱記憶體序（Weak Memory Ordering）：  
CPU 可以重新排序讀寫操作（為了效能）。在嵌入式安全場景需要明確的 barrier：

```c
// DSB：Data Synchronization Barrier
// 確保之前的所有記憶體操作完成後才繼續
__DSB();   // Cortex-M: __DSB(); A64: asm("dsb sy")

// ISB：Instruction Synchronization Barrier
// 清除 pipeline，確保之後的指令從新的狀態取指
__ISB();

// DMB：Data Memory Barrier
// 確保記憶體存取的相對順序（比 DSB 弱）
__DMB();

// 典型用法：設定 MPU/SAU 後必須加
SAU->CTRL = SAU_CTRL_ENABLE_Msk;
__DSB();   // 確保 SAU 設定生效
__ISB();   // 確保之後的指令都在新的 SAU 設定下執行
```

---

## 對 RoT 的意義

```
攻擊者目標：讓 A35 在 M33 驗證完成前就開始執行（或跳過驗證）

防護設計：
1. M33 用 MPU 保護 Secure SRAM（key 不可被 NS 讀取）
2. A35 Reset 線由 M33 的 GPIO 控制
3. 驗證失敗 → M33 永遠不 release Reset → A35 卡死
4. M33 本身的程式碼在 Secure Flash（NS 不可讀，防 dump）
5. 驗證完成前，A35 的 SRAM 映射到虛擬位址 0（或保持 Reset 狀態）
```

---

## 下一步

→ 完成 ARM 架構模組，繼續 [boot-flow/](../boot-flow/README.md)
