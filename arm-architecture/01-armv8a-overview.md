---
tags: [arm-architecture, module]
topic: arm-architecture
week: "3-4"
---
# ARM 架構 Module 1：ARMv8-A 概覽

## 為什麼要學 ARMv8-A

STM32MP215F-DK 的 Cortex-A35 是 ARMv8-A 架構，執行 TF-A、U-Boot 和 Linux。  
理解它的暫存器、指令集、特權模型，才能看懂 boot flow 和 TrustZone 原理。

---

## ARMv8-A 執行狀態

ARMv8-A 有兩種執行狀態（Execution State）：

| 狀態 | 位元寬 | 指令集 | 使用時機 |
|------|--------|--------|---------|
| **AArch64** | 64-bit | A64 | 現代 OS、TF-A、U-Boot |
| **AArch32** | 32-bit | A32/T32（Thumb） | 舊 32-bit 程式碼相容 |

Cortex-A35 預設在 AArch64 開機，但可以切換到 AArch32（例如執行舊版 Linux）。  
本專案的 TF-A 和 Linux 都用 AArch64。

---

## AArch64 暫存器

**暫存器是什麼：** CPU 內部的極小記憶體格子，速度比 RAM 快幾百倍。CPU 做計算時只能用暫存器，不能直接對 RAM 做加法。你的變數在 RAM，運算前要先搬進暫存器，算完再搬回去。

```
CPU 內部（暫存器）：
┌────┬────┬────┬────┬────┐
│ X0 │ X1 │ X2 │ X3 │... │  ← 速度極快，但只有幾十個
└────┴────┴────┴────┴────┘

RAM（記憶體）：
┌──────────────────────────────────────┐
│  variables, arrays, stack, heap...    │  ← 慢，但有幾 GB
└──────────────────────────────────────┘
```

### 通用暫存器

```
X0–X30   64-bit 通用暫存器（存任何數值）
W0–W30   同一批暫存器的低 32-bit 視圖（W0 和 X0 是同一個，只看低半部）
```

**呼叫慣例（Calling Convention）**：函式間傳資料用哪些暫存器的規定，讓 C 和組語可以互相呼叫：

```
X0–X7    傳參數進去，回傳值傳出來（最多 8 個參數）
X9–X15   Caller-saved：呼叫別人前要自己備份，對方可能會改掉
X19–X28  Callee-saved：被呼叫的函式用完後必須還原（不能留下改過的值）
X29      Frame Pointer（FP）：指向目前函式的 stack frame 起點
X30      Link Register（LR）= 函式的返回位址（RET 跳到這裡）
```

```
int add(int a, int b) { return a + b; }
呼叫 add(3, 5)：
  → 3 放進 X0，5 放進 X1（參數）
  → 執行 add
  → 結果 8 放進 X0（回傳值）
  → 呼叫者從 X0 讀結果
```

### 特殊暫存器

```
PC       Program Counter = 下一條要執行的指令位址（CPU 自動更新）
SP       Stack Pointer = 目前 stack 頂部的位址
PSTATE   程式狀態旗標：
           N = 上次運算結果是負數
           Z = 上次運算結果是零
           C = 進位
           V = 溢位
           EL = 目前的 Exception Level（0~3）
```

### 系統暫存器（透過 MSR/MRS 存取）

```c
// 讀取目前 Exception Level
uint64_t el;
asm("mrs %0, CurrentEL" : "=r"(el));
el = (el >> 2) & 3;  // bit [3:2] 是 EL 值

// 讀取 CPU ID
uint64_t mpidr;
asm("mrs %0, MPIDR_EL1" : "=r"(mpidr));
```

---

## A64 常用指令

```asm
// 資料搬移
MOV  X0, #42          // X0 = 42
MOV  X1, X0           // X1 = X0
LDR  X0, [X1]         // X0 = Memory[X1]（載入 64-bit）
STR  X0, [X1]         // Memory[X1] = X0（儲存 64-bit）
LDR  W0, [X1]         // 載入 32-bit（W 暫存器）
LDRB W0, [X1]         // 載入 8-bit

// 算術 / 邏輯
ADD  X0, X1, X2       // X0 = X1 + X2
SUB  X0, X1, #4       // X0 = X1 - 4
AND  X0, X1, X2       // X0 = X1 & X2
ORR  X0, X1, X2       // X0 = X1 | X2
LSL  X0, X1, #3       // X0 = X1 << 3
LSR  X0, X1, #3       // X0 = X1 >> 3（邏輯右移）

// 跳轉
B    label            // 無條件跳轉
BL   func             // 呼叫（LR = 返回位址）
RET                   // 返回（跳到 LR）
BR   X0               // 跳到 X0 的位址（間接跳轉）
BLR  X0               // 呼叫 X0 的位址

// 條件跳轉
CMP  X0, #0           // 比較（設 PSTATE 旗標）
BEQ  label            // 相等時跳轉（Z=1）
BNE  label            // 不相等時跳轉（Z=0）
BGT  label            // 大於
BLT  label            // 小於
```

---

## 記憶體存取與對齊

AArch64 的記憶體存取**要求對齊**（aligned access）：

```
LDR X0 從位址 0x1000  → OK（8-byte 對齊）
LDR X0 從位址 0x1004  → 可能 Alignment Fault
LDR W0 從位址 0x1002  → 可能 Alignment Fault（4-byte 不對齊）
LDRB W0 從任意位址   → 永遠 OK（1-byte 無對齊要求）
```

嵌入式程式碼要注意 struct 的 padding 和 `__attribute__((packed))` 的影響。

---

## 呼叫慣例（AAPCS64）

```c
// C 函式對應的 AArch64 約定
long result = func(a, b, c, d, e, f, g, h);

// 組語層面：
// 參數 1–8 放 X0–X7
// 超過 8 個放 Stack
// 回傳值放 X0（64-bit）或 X0+X1（128-bit）
// X19–X28 若函式用到，必須在返回前還原
```

---

## Cortex-A35 vs Cortex-A53

STM32MP215F 用的是 Cortex-A35（不是 A53）：

| 規格 | Cortex-A35 | Cortex-A53 |
|------|-----------|-----------|
| 架構 | ARMv8-A | ARMv8-A |
| Pipeline | 8-stage | 8-stage |
| 功耗 | 更低（~0.13 mW/MHz/core） | 較高 |
| 效能 | 較低 | 較高 |
| 應用 | IoT、嵌入式 Linux | 手機、邊緣裝置 |

A35 是 ARMv8-A 中效能最低、功耗最省的，適合 IoT 和嵌入式場景。

---

## Cache 基本概念

**Cache 是什麼：** CPU 和 RAM 之間的中間層快取記憶體。RAM 速度比 CPU 慢幾十倍，Cache 把常用的資料存在 CPU 旁邊，讓 CPU 不用每次都等 RAM：

```
存取速度（快 → 慢）：
CPU 暫存器 < L1 Cache < L2 Cache < RAM（DDR）< Flash（NOR）

  L1 Cache：幾十 KB，幾個 cycle
  L2 Cache：幾百 KB，十幾個 cycle
  RAM：幾 GB，幾十個 cycle
  Flash：幾十 MB，幾百個 cycle
```

Cortex-A35 有 L1/L2 Cache，和 MCU（Cortex-M33）不同。  
Cache 帶來效能，但也帶來以下安全問題：

```
問題 1：Cache coherency（快取一致性）
  M33 寫入共享 RAM → A35 的 Cache 還是舊值 → A35 讀到錯誤資料
  解法：A35 在讀取前執行 cache invalidate（讓 cache 失效，強迫重新讀 RAM）

問題 2：Cache side-channel（快取旁路攻擊）
  攻擊者可透過測量 cache 存取時間來猜測 key（如 Spectre/Meltdown）
  在 RoT 設計中，Secure 側的 key 不應進入 A35 可見的 cache
```

---

## 下一步

→ [Module 2：Exception Levels](02-exception-levels.md)
