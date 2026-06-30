---
tags: [trustzone, module]
topic: trustzone
week: "5-6"
---
# TrustZone Module 2：A-profile TrustZone（A35 側）

## 硬體元件

A35 TrustZone 由三個硬體元件協作，各自負責不同的隔離：

```
CPU (Cortex-A35)
  └── SCR_EL3: control register at EL3 — sets current World & IRQ routing

TZASC (TrustZone Address Space Controller)   <-- gatekeeper between CPU & DDR
  └── every DDR access passes through TZASC
      NS requests to Secure DDR regions are blocked

TZMA (TrustZone Memory Adapter) / ETZPC      <-- protects SRAM & peripherals
  └── each peripheral can be set Secure-only or accessible by both Worlds
      (UART, SPI, RNG ...)

GIC (Generic Interrupt Controller)           <-- interrupts have security attrs
  └── each IRQ can be routed to Secure World (FIQ) or Normal World (IRQ)
      secure timer / secure UART interrupts are invisible to Normal World
```

**為什麼中斷也要分安全屬性：** 如果 UART 中斷可以被 Normal World 攔截，攻擊者可以透過中斷機制干擾 Secure World 的執行流。GIC 讓 Secure 的硬體事件（如 tamper 偵測）直接路由到 EL3，Normal World 看不到。

---

## SCR_EL3（Secure Configuration Register）

BL31 用 SCR_EL3 控制 TrustZone 的行為：

```c
// SCR_EL3 重要 bits
#define SCR_NS_BIT    (1 << 0)   // 0=Secure, 1=Non-Secure（當前世界）
#define SCR_IRQ_BIT   (1 << 1)   // 1=IRQ 路由到 EL3
#define SCR_FIQ_BIT   (1 << 2)   // 1=FIQ 路由到 EL3（Secure 中斷用）
#define SCR_EA_BIT    (1 << 3)   // External Abort 路由到 EL3
#define SCR_RW_BIT    (1 << 10)  // 1=EL1/EL0 用 AArch64
#define SCR_HCE_BIT   (1 << 8)   // 1=允許 EL2 HVC 指令

// BL31 跳到 Linux（Non-Secure）前：
SCR_EL3 |= SCR_NS_BIT;    // 設 NS=1
SCR_EL3 |= SCR_FIQ_BIT;   // FIQ 給 Secure World
SCR_EL3 &= ~SCR_IRQ_BIT;  // IRQ 給 Normal World
```

---

## TZASC（TrustZone Address Space Controller）

保護 DDR 中的 Secure 記憶體區域：

DDR 1 GB：0x8000_0000 – 0xBFFF_FFFF，經 TZASC 分為兩個 Region：
```
            DDR 0x8000_0000 - 0xBFFF_FFFF (1 GB)
                         |
                       TZASC
                         |
  ┌──────────────────────┴─────────────────────┐
  │ Region 1: OP-TEE (Secure)                  │ <-- Secure World only
  │ 0xFE00_0000 - 0xFFFF_FFFF                  │
  ├────────────────────────────────────────────┤
  │ Region 0: Linux (NS)                       │ <-- Normal World accessible
  │ 0x8000_0000 - 0xFDFF_FFFF                  │
  └────────────────────────────────────────────┘
```

TF-A BL31 在初始化時配置 TZASC：

```c
// TF-A 平台程式碼：plat/st/stm32mp2/bl31_plat_setup.c
void bl31_platform_setup(void) {
    // 配置 TZASC region
    // Region 0: 預設 Non-Secure（DDR 大部分）
    // Region 1: Secure（OP-TEE 使用，128 MB）
    tzasc_configure_region(
        TZASC_BASE,
        1,                          // region 1
        0xFE000000,                 // base
        TZASC_ATTR_SECURE_RW,       // Secure Read/Write
        0x2000000                   // 32 MB
    );
}
```

---

## 世界切換詳細過程

```c
// Normal World（Linux）發出 SMC：
//   SMC #0
//   X0 = 0x32000004（OP-TEE call with arg）
//   X1 = params 的實體位址

// BL31 的 SMC handler 被呼叫：
void smc_handler(uint64_t func_id, ...) {
    if (func_id == PSCI_xxx) {
        psci_handler(...);
    } else if (func_id & OPTEE_SMC_MASK) {
        // 切換到 Secure World
        // 1. 儲存 Non-Secure 的 context（暫存器、SP、PC）
        cm_save_context(&ns_ctx);

        // 2. 載入 Secure context
        cm_restore_context(&s_ctx);

        // 3. 設定 SCR_EL3.NS = 0
        SCR_EL3 &= ~SCR_NS_BIT;

        // 4. ERET 到 OP-TEE
        // OP-TEE 完成後用 SMC 返回 BL31

        // 5. 恢復步驟（BL31 收到 OP-TEE 的 SMC 後）
        cm_save_context(&s_ctx);
        cm_restore_context(&ns_ctx);
        SCR_EL3 |= SCR_NS_BIT;
        // ERET 回 Linux
    }
}
```

---

## GIC（Generic Interrupt Controller）配置

ARM GIC（通常是 GIC-400 或 GIC-500）控制中斷的安全屬性：

```c
// 每個 SPI（Shared Peripheral Interrupt）可設定 Secure 或 Non-Secure
// Group 0 = Secure FIQ
// Group 1 = Non-Secure IRQ

// 讓 Secure Timer（TIM1）的中斷只到 Secure World
GICD->IGROUPR[0] &= ~(1 << TIM1_IRQn);  // Group 0 = Secure

// 讓 UART（USART2）的中斷到 Normal World
GICD->IGROUPR[1] |= (1 << USART2_IRQn); // Group 1 = Non-Secure
```

---

## TZMA：Peripheral 安全屬性

STM32MP2 的每個 peripheral 可以被標記為 Secure 或 Non-Secure：

```c
// ETZPC（Extended TrustZone Protection Controller）
// STM32MP 特有，控制 peripheral 的 Secure/NS 屬性

// 讓 RNG（Random Number Generator）只有 Secure World 能用
ETZPC->DECPROT[0] |= DECPROT_SECURE(RNG_IDX);

// 讓 UART 給 Normal World
ETZPC->DECPROT[1] |= DECPROT_NS(USART2_IDX);
```

---

## 記憶體映射安全屬性（MAIR + 頁表）

TF-A 在 BL2/BL31 的頁表中設定記憶體安全屬性：

```c
// 頁表 descriptor 中的安全相關 bits
// Bit[5] NS：Non-Secure bit
//   0 = Secure（只有 Secure EL 能用這個映射）
//   1 = Non-Secure

// Secure 記憶體的頁表描述符（NS=0）
uint64_t secure_mem_entry = pa | PTE_VALID | PTE_TABLE;
// NS bit 不設 → Secure

// Non-Secure 記憶體的頁表描述符（NS=1）
uint64_t ns_mem_entry = pa | PTE_VALID | PTE_TABLE | PTE_NS;
```

---

## 下一步

→ [Module 3：M-profile TrustZone（M33 側）](03-tz-m-profile.md)
