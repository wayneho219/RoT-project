---
tags: [arm-architecture, module]
topic: arm-architecture
week: "3-4"
---
# ARM 架構 Module 2：Exception Levels 與安全狀態

## 核心概念

ARMv8-A 的特權模型：**Exception Level（EL）** 決定軟體能做什麼，**Security State** 決定能看到什麼。

```
EL3（最高特權）→ Secure Monitor（TF-A BL31）
EL2            → Hypervisor（可選）
EL1            → OS kernel（Linux / BL2 / U-Boot）
EL0（最低特權）→ User space app

每個 EL 都有 Secure 和 Non-Secure 兩種狀態（EL3 只有 Secure）
```

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

高 EL 跳到低 EL 用 `ERET`（Exception Return）：

```asm
// BL31 跳到 BL33（U-Boot）的大略邏輯
MSR  ELR_EL3, X0     // 設定返回位址（U-Boot 的入口）
MSR  SPSR_EL3, X1    // 設定目標的 PSTATE（EL1-NS）
ERET                 // 跳過去
```

不能直接 `B` 或 `BL` 到低 EL——ERET 是唯一合法的降級路徑。

---

## 異常（Exception）的種類

進入高 EL（特權提升）的方式：

```
SVC   → EL0 呼叫 EL1（syscall）
HVC   → EL1 呼叫 EL2（hypercall）
SMC   → 任何 EL 呼叫 EL3（secure monitor call）
IRQ   → 中斷（Interrupt Request）
FIQ   → 快速中斷（通常路由到 Secure World）
SError → 非同步異常（如 bus error）
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
