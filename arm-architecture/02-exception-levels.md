---
tags: [arm-architecture, module]
topic: arm-architecture
week: "3-4"
---
# ARM 架構 Module 2：Exception Levels 與安全狀態

## 核心概念

**Exception Level（特權層級）是什麼：** CPU 執行時有不同的「權限等級」，高等級可以做更多事（改硬體設定、存取所有記憶體），低等級受到限制。這是為了安全隔離——如果 user app 有 bug，它不能破壞整個系統。

「Exception」這個名稱來自「跳到高特權層級」的機制叫做 exception（中斷、syscall 等），所以這個層級模型叫做 Exception Level。

**為什麼要分四層：**

```
問題：如果所有軟體都有最高權限，任何 bug 都是災難性的
解法：按「信任程度」分層，下層不能影響上層

EL3（最高特權）→ Secure Monitor（TF-A BL31）
EL2            → Hypervisor（可選，管理虛擬機）
EL1            → OS kernel（Linux / U-Boot）
EL0（最低特權）→ User space app（你的程式）

每個 EL 都有 Secure 和 Non-Secure 兩種狀態（EL3 只有 Secure）
```

**與 Windows/Linux 的對應：**

```
Linux 概念：         ARM EL 對應：
kernel space    ↔   EL1（可存取所有硬體暫存器）
user space      ↔   EL0（只能用 syscall 請 kernel 幫忙）
hypervisor      ↔   EL2（管多個 OS，本專案不用）
secure monitor  ↔   EL3（比 kernel 更高，管 TrustZone）
```

ARMv8-A 的特權模型：**EL** 決定軟體能做什麼，**Security State** 決定能看到什麼。

---

## Exception Levels 詳解

### EL3：Secure Monitor

- 整個系統最高權限
- 由 **TF-A BL31** 常駐在這裡
- 負責在 Secure World（EL1-S、EL0-S）和 Normal World（EL1-NS、EL0-NS）之間切換
- 管理 PSCI（電源管理）、SMC handler

```
EL3 是 TrustZone 的"守門人"。
Non-Secure 軟體無法提升到 Secure World，除非透過 SMC 呼叫 EL3。
```

### EL2：Hypervisor

- 管理虛擬機（VM）
- 本專案不使用 Hypervisor，但 TF-A 會初始化 EL2 再跳到 BL33

### EL1：OS / 韌體

- Secure EL1：TrustZone Trusted OS（例如 OP-TEE kernel）
- Non-Secure EL1：Linux kernel、U-Boot

### EL0：User space

- Secure EL0：Trusted Application（在 OP-TEE 環境執行的安全程式）
- Non-Secure EL0：一般 Linux 程式

---

## Security State

ARMv8-A 有兩個 Security State：

```
Secure World（SW）              Non-Secure World（NW）
─────────────────               ──────────────────────
EL3（BL31 Secure Monitor）
EL1-S（OP-TEE kernel）          EL1-NS（Linux kernel）
EL0-S（Trusted App）            EL0-NS（Linux user app）

Secure World 可以存取所有記憶體
Non-Secure World 只能存取被標記為 Non-Secure 的記憶體
```

### SCR_EL3.NS bit

由 EL3 設定，決定目前在哪個世界：

```c
// TF-A BL31 切換到 Non-Secure EL1（Linux）前設定
SCR_EL3 |= SCR_NS_BIT;   // NS = 1 → Non-Secure World
```

---

## 世界切換（World Switch）

```
Non-Secure EL1（Linux）
    │
    │  SMC（Secure Monitor Call）
    ▼
EL3（BL31 Secure Monitor）
    │
    │  設定 SCR_EL3.NS = 0，ERET 到 Secure EL1
    ▼
Secure EL1（OP-TEE）
    │
    │  完成後 SMC 返回 EL3
    ▼
EL3（BL31）
    │
    │  設定 SCR_EL3.NS = 1，ERET 回 Linux
    ▼
Non-Secure EL1（Linux）
```

**SMC（Secure Monitor Call）**：Non-Secure 觸發世界切換的唯一方式。  
就像 `syscall` 之於 user/kernel 切換，`SMC` 之於 NW/SW 切換。

---

## ERET：特權降低的方式

**為什麼不能直接跳轉：** 如果 EL3 可以用 `B`（普通跳轉）到 EL1，那 EL1 的程式碼也能偽造這個跳轉，假裝自己從 EL3 跳過來，繞過安全檢查。`ERET` 是硬體規定的唯一降級路徑，只有高 EL 才能執行，低 EL 執行 ERET 會觸發 fault。

高 EL 跳到低 EL 用 `ERET`（Exception Return）：

```asm
// BL31 跳到 BL33（U-Boot）的大略邏輯
MSR  ELR_EL3, X0     // 設定「返回後要去的位址」（U-Boot 的入口）
MSR  SPSR_EL3, X1    // 設定「返回後的狀態」（EL1-NS，Non-Secure）
ERET                 // 降到 EL1-NS，跳到 ELR_EL3 的位址
```

```
ERET 的效果：
  EL3 → 設定目標 EL 和位址 → ERET → 硬體切換到 EL1，跳到指定位址
  低 EL 無法自己呼叫 ERET 升級，只能透過 SMC 請 EL3 幫忙
```

不能直接 `B` 或 `BL` 到低 EL——ERET 是唯一合法的降級路徑。

---

## 異常（Exception）的種類

進入高 EL（特權提升）的方式。注意：這裡的「異常」不是錯誤，而是「觸發 CPU 跳到高特權層級處理」的事件：

```
SVC    → EL0 請求 EL1 服務（= Linux syscall，如 read()、write()）
HVC    → EL1 請求 EL2 服務（= hypercall，VM 請 hypervisor 幫忙）
SMC    → 任何 EL 請求 EL3 服務（= secure monitor call，NW 請 SW 幫忙）
IRQ    → 硬體中斷（如 UART 資料到了、Timer 到期）
FIQ    → 快速中斷（通常路由到 Secure World，NW 看不到）
SError → 非同步錯誤（如 bus error，記憶體存取失敗）

類比：
SVC → C# 的方法呼叫（user → kernel）
SMC → kernel 請求更底層的 secure firmware
IRQ → C# 的 event 觸發（硬體事件 → CPU）
```

### FIQ vs IRQ 路由

TrustZone 的重要設計：FIQ 可以設定為只被 Secure World 處理：

```
SCR_EL3.FIQ = 1  → FIQ 直接路由到 EL3，NW 看不到
SCR_EL3.IRQ = 0  → IRQ 在 NW 處理（Normal World）
```

Secure 硬體（例如 RoT timer、Secure UART）可以用 FIQ，確保 NW 無法攔截。

---

## 向量表（Vector Table）

每個 EL 有一個向量表（VBAR_ELn），記錄各種異常的入口位址：

```c
// TF-A BL31 設定向量表
extern void bl31_vector_table;
asm("msr VBAR_EL3, %0" : : "r"(&bl31_vector_table));

// 向量表結構（每個入口 128 bytes）
// Current EL with SP0
//   Sync, IRQ, FIQ, SError
// Current EL with SPx
//   Sync, IRQ, FIQ, SError
// Lower EL using AArch64
//   Sync, IRQ, FIQ, SError
// Lower EL using AArch32
//   Sync, IRQ, FIQ, SError
```

---

## STM32MP215F-DK 的 Boot EL 流程

```
上電
  │
  ▼
ROM Code（EL3，Secure）
  │  載入並驗證 BL2
  ▼
TF-A BL2（EL1-S，Secure）
  │  載入 BL31, BL33
  ▼
TF-A BL31（EL3，Secure）─── 常駐，作為 Secure Monitor
  │  ERET to U-Boot
  ▼
U-Boot（EL1-NS，Non-Secure）
  │
  ▼
Linux kernel（EL1-NS）
  │
  ▼
User space（EL0-NS）
```

詳細 boot flow 見 `boot-flow/` 目錄。

---

## 下一步

→ [Module 3：Cortex-M33 架構](03-cortex-m33.md)
