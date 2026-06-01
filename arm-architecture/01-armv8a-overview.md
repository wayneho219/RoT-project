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

### 通用暫存器

```
X0–X30   64-bit 通用暫存器
W0–W30   同一批暫存器的低 32-bit 視圖

X0–X7    函式參數 / 回傳值（Calling Convention）
X9–X15   暫時用（Caller-saved）
X19–X28  要保留的（Callee-saved，函式返回前要還原）
X29      Frame Pointer（FP）
X30      Link Register（LR）= 函式返回位址
```

### 特殊暫存器

```
PC       Program Counter（不能直接讀寫）
SP       Stack Pointer（每個 EL 各有一個）
PSTATE   狀態旗標（N/Z/C/V、EL、Security state）
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

Cortex-A35 有 L1/L2 Cache，和 MCU（Cortex-M33）不同。  
Cache 帶來效能，但也帶來以下安全問題：

```
問題 1：Cache coherency
  M33 和 A35 共享記憶體時，A35 可能讀到 cache 裡的舊資料
  解法：在 M33 側寫完後，A35 需要 invalidate cache

問題 2：Cache side-channel
  攻擊者可透過 cache timing 洩漏 key（如 Spectre/Meltdown）
  在 RoT 設計中，Secure 側的 key 不應進入 A35 可見的 cache
```

---

## 下一步

→ [Module 2：Exception Levels](02-exception-levels.md)
