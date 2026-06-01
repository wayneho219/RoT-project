---
tags: [arm-architecture, index]
topic: arm-architecture
week: "3-4"
---
# ARM 架構學習筆記

## 本模組對應的硬體

STM32MP215F-DK 上有兩顆 ARM 核心：

| 核心 | 架構 | 角色 |
|------|------|------|
| Cortex-A35 | ARMv8-A（64-bit） | Application Processor，執行 Linux |
| Cortex-M33 TD | ARMv8-M（32-bit） | RoT MCU，最先上電、驗證 firmware |

理解這兩顆核心的架構，是實作 Root of Trust 的基礎。

## 閱讀順序

| 模組 | 主題 |
|------|------|
| [01-armv8a-overview](01-armv8a-overview.md) | ARMv8-A 基本概念、暫存器、指令集 |
| [02-exception-levels](02-exception-levels.md) | EL0–EL3 特權層級、安全/非安全世界 |
| [03-cortex-m33](03-cortex-m33.md) | ARMv8-M、Cortex-M33 特性、SAU/IDAU |
| [04-memory-protection](04-memory-protection.md) | MMU（A-profile）vs MPU（M-profile） |

## 與 RoT 的關係

```
M33-TD（ARMv8-M）
 ├── 上電即執行，比 A35 更早
 ├── SAU 劃分 Secure / Non-Secure 區域
 ├── 從 Secure SRAM 執行驗證邏輯
 └── 驗證通過 → 寫暫存器 release A35 reset

A35（ARMv8-A）
 ├── 從 Reset 釋放後開始執行
 ├── EL3（BL31 Secure Monitor）
 ├── EL2（Hypervisor，可選）
 ├── EL1（BL2、U-Boot、Linux kernel）
 └── EL0（User space）
```
