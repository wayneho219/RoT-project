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

**MMU 是什麼：** Memory Management Unit，把「虛擬位址」翻譯成「實體位址」的硬體單元。

**為什麼需要虛擬位址：**

```
問題：多個 process 同時執行，彼此不能互相讀取記憶體
      Process A 的程式碼都假設自己從 0x400000 開始
      Process B 也假設自己從 0x400000 開始
      → 兩個 process 不能同時存在同一個實體位址

解法：給每個 process 一個「假的位址空間」（虛擬位址）
      OS 背後維護一張表（頁表），記錄虛擬 → 實體的對應
      Process A 的 VA 0x400000 → 實體 0x80001000
      Process B 的 VA 0x400000 → 實體 0x90002000
      互不干擾
```

```
Process A 存取 VA 0x400000：
  → MMU 查頁表 → 找到 PA 0x80001000 → 去 RAM 讀資料

Process A 存取 VA 0x1000（沒有映射）：
  → MMU 查頁表 → 找不到 → Page Fault → OS 介入處理（通常是 crash）
```

### 虛擬位址映射

Linux 在 A35 上執行時，每個 process 都有自己的虛擬位址空間：

```
虛擬位址空間（64-bit user space）
  0x0000_0000_0000_0000 – 0x0000_FFFF_FFFF_FFFF  → User space（每個 process 各自的空間）
  0xFFFF_0000_0000_0000 – 0xFFFF_FFFF_FFFF_FFFF  → Kernel space（所有 process 共用）

實體記憶體（STM32MP215F-DK）
  0x8000_0000 – 0xBFFF_FFFF  → DDR（1 GB，Linux 用）
  0x0E00_0000 – 0x0EFF_FFFF  → Secure SRAM（M33 用，A35 看不到）
  0x6000_0000 – 0x61FF_FFFF  → FMC 外部記憶體空間（板上未接 NOR Flash）
```

### 頁表（Page Table）

```
頁（page）= MMU 映射的最小單位，固定 4 KB（0x1000）
頁表（page table）= 記錄「哪個虛擬頁對應到哪個實體頁」的表格

VA 0x400000 → PA 0x80001000  （可讀可執行）
VA 0x401000 → PA 0x80002000  （可讀可執行，下一頁）
VA 0x1000   → 無映射         → Page Fault（段錯誤，通常讓程式 crash）
```

Boot 期間，TF-A 和 U-Boot 用 **恆等映射**（identity mapping）：  
`VA == PA`，省掉翻譯，簡化程式碼。Linux 啟動後才建立完整頁表。

### TLB（Translation Lookaside Buffer）

每次記憶體存取都查頁表很慢，TLB 是頁表的快取：

```
第一次存取 VA 0x400000：
  → MMU 查頁表（慢）→ 找到 PA → 把這個對應存進 TLB

第二次存取 VA 0x400000：
  → MMU 先查 TLB（快）→ 命中 → 直接用 PA，不用查頁表
```

切換 process 時，TLB 必須清空（舊 process 的對應對新 process 無效）：

```c
asm("tlbi vmalle1");    // 讓 EL1 的所有 TLB 失效（切換 process 或 world 時）
asm("dsb sy");          // 確保 TLB 清空完成
asm("isb");             // 清除 pipeline，確保後續指令用新的 TLB
```

### Memory Attributes（MAIR_EL1）

不同記憶體區域有不同特性，MMU 頁表裡每個頁都標記屬性：

```
Device-nGnRnE  → 硬體暫存器（MMIO）
                  不可 cache（寫進去要立刻生效，不能被快取）
                  不可 reorder（寫的順序要和程式碼一樣）

Normal         → 一般記憶體（DDR）
                  可以 cache，CPU 可以重排存取順序（效能最好）

Normal NC      → Non-Cacheable DDR
                  M33 和 A35 共享記憶體用，避免 cache coherency 問題
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

**為什麼 CPU 會亂序：** ARM 是「弱記憶體序（Weak Memory Ordering）」架構，CPU 和編譯器為了效能，可以把讀寫操作重新排序。大多數時候無所謂，但在安全關鍵設定（MPU、SAU）上，順序錯了會有漏洞：

```
你寫的程式碼：         CPU 實際執行的順序（可能）：
SAU->CTRL = enable;   後面的程式碼先跑了
後面的程式碼...        SAU->CTRL = enable;  ← 設定還沒生效就繼續執行！
```

加上 barrier 告訴 CPU「這裡不准重排」：

```c
SAU->CTRL = SAU_CTRL_ENABLE_Msk;
__DSB();   // Data Synchronization Barrier：等到這行前的所有記憶體操作完成
__ISB();   // Instruction Synchronization Barrier：清除 pipeline，確保後續指令在新狀態下執行

// 這之後的程式碼才開始跑，SAU 設定已確實生效
```

三種 barrier 的強弱：

```
DSB（最強）：等所有記憶體操作完成，再繼續任何指令
DMB（中間）：只確保記憶體存取的相對順序，不等其他指令
ISB（pipeline）：清除 CPU pipeline，讓後續指令重新取指（用於改完設定後）

典型組合：
  設定完 MPU/SAU/SCR → __DSB() + __ISB()
  多核共享資料同步   → __DMB()
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
