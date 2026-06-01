---
tags: [arm-architecture, module]
topic: arm-architecture
week: "3-4"
---
# ARM 架構 Module 3：Cortex-M33（ARMv8-M）

## 為什麼 M33 是 RoT

STM32MP215F-DK 上的 Cortex-M33 TD（Trusted Domain）：

- **比 A35 更早上電**，ROM code 直接在 M33 執行
- **架構更簡單**：沒有 Cache、沒有 MMU，執行流程可預測
- **ARMv8-M TrustZone**：硬體隔離 Secure / Non-Secure 區域
- 持有 Key，A35 無法直接存取 M33 的 Secure SRAM

---

## ARMv8-M vs ARMv8-A

| 特性 | ARMv8-A（A35）| ARMv8-M（M33）|
|------|--------------|--------------|
| 指令集 | A64（64-bit）| Thumb-2（32-bit）|
| 特權模型 | EL0–EL3 | Thread / Handler |
| TrustZone | NS/S EL | SAU + IDAU |
| Cache | L1/L2 | 通常無（或可選） |
| MMU | 有（分頁式） | 無 |
| MPU | 可選 | 有（最多 16 region）|
| 中斷模型 | GIC | NVIC |
| 開機順序 | 需要 bootloader | 直接從 Flash vector 表 |

---

## Cortex-M33 執行模式

```
Thread Mode（用戶/一般程式碼）
  ├── Privileged Thread：main() 和大多數程式碼
  └── Unprivileged Thread：可選，OS 的 task

Handler Mode（ISR，中斷服務程式）
  └── 永遠是 Privileged
```

**與 A35 最大的不同**：M33 只有 2 個特權層，A35 有 4 個。

---

## Cortex-M33 暫存器

```
R0–R12    通用暫存器
R13 (SP)  Stack Pointer（MSP / PSP，兩個）
R14 (LR)  Link Register（返回位址 / EXC_RETURN 特殊值）
R15 (PC)  Program Counter
xPSR      程式狀態暫存器（N/Z/C/V 旗標 + 中斷號）

MSP  Main Stack Pointer（Handler Mode 用）
PSP  Process Stack Pointer（Thread Mode 用，OS 任務切換）
```

---

## ARMv8-M TrustZone：SAU 和 IDAU

M33 的 TrustZone 靠兩個機制劃分記憶體：

### SAU（Security Attribution Unit）

軟體配置，最多 8 個 region：

```c
// 設定 SAU region 0：Flash 0x0800_0000–0x0807_FFFF 為 Secure
SAU->RBAR = 0x08000000 & SAU_RBAR_BADDR_Msk;
SAU->RLAR = (0x0807FFFF & SAU_RLAR_LADDR_Msk) | SAU_RLAR_ENABLE_Msk;

// 選擇 region 0 並啟用
SAU->RNR  = 0;
// 啟用 SAU（否則所有記憶體視為 Non-Secure）
SAU->CTRL = SAU_CTRL_ENABLE_Msk;
```

### IDAU（Implementation Defined Attribution Unit）

硬體固定，由 ST 在 STM32MP215F 出廠時定義。  
IDAU 優先於 SAU，可以強制把某些區域永遠是 Secure。

```
結合規則：
Secure   = SAU 說是 Secure  OR  IDAU 說是 Secure
Non-Secure-Callable = SAU 說是 NSC
Non-Secure = 其他
```

---

## Secure / Non-Secure 呼叫介面

M33 TrustZone 的世界切換和 A35 不同，是函式呼叫層級的：

### Secure → Non-Secure：BXNS / BLXNS

```c
// 從 Secure code 呼叫 Non-Secure 函式
typedef void (*NS_func)(void) __attribute__((cmse_nonsecure_call));

NS_func ns_entry = (NS_func)(0x20010001); // LSB=1 表示 Thumb
ns_entry();   // BLXNS 指令，自動清除 LR 的 Secure bit
```

### Non-Secure → Secure：CMSE_NONSECURE_ENTRY

```c
// Secure 側開放給 Non-Secure 呼叫的函式（放在 NSC 區域）
__attribute__((cmse_nonsecure_entry))
uint32_t secure_get_version(void) {
    // 在這裡可以存取 Secure 資源
    return FIRMWARE_VERSION;
}

// Non-Secure 側看到的只是一個函式指標
// 呼叫時 CPU 透過 BXNS 機制切換到 Secure
```

**NSC（Non-Secure Callable）** 區域：是 Secure 程式碼裡的「入口大廳」，  
Non-Secure 只能跳進 NSC 區域，不能直接跳到其他 Secure 程式碼。

---

## NVIC（巢狀向量中斷控制器）

M33 的中斷完全由 NVIC 管理：

```c
// 啟用中斷
NVIC_EnableIRQ(TIM2_IRQn);
NVIC_SetPriority(TIM2_IRQn, 3);  // 優先級 0 最高

// ISR 命名規則（對應到向量表）
void TIM2_IRQHandler(void) {
    // 清 pending flag，否則會一直觸發
    TIM2->SR &= ~TIM_SR_UIF;
}

// 向量表（startup.s 定義，或直接用 SCB->VTOR）
SCB->VTOR = 0x20000000;  // 把向量表搬到 SRAM（例如更新後的 OTA）
```

---

## M33 開機流程

```
上電 / Reset
  │
  ▼
從 Flash[0] 讀取 MSP（Main Stack Pointer）初始值
從 Flash[4] 讀取 Reset_Handler 位址
  │
  ▼
Reset_Handler（startup.s）
  ├── 初始化 .data（複製 ROM → RAM）
  ├── 初始化 .bss（清零）
  ├── 呼叫 SystemInit()（時鐘、快取配置）
  └── 呼叫 main()
        │
        ▼
      main()
        ├── 配置 SAU（劃分 Secure/NS 記憶體）
        ├── 配置 MPU（存取保護）
        ├── 從 NOR Flash 讀取 A35 firmware header
        ├── 計算 SHA-256
        ├── 驗證 ECDSA 簽章
        ├── 通過 → 寫 RCC 暫存器，release A35 reset
        └── 失敗 → 鎖死（無限迴圈 / 關閉 A35）
```

---

## 重要：M33 沒有 MMU

沒有分頁機制，記憶體保護完全靠 MPU：

```c
// 設定 MPU region：Secure SRAM 對 NS 不可存取
MPU->RNR  = 0;                              // region 0
MPU->RBAR = 0x20000000 | MPU_RBAR_S_Msk;   // base + Secure attribute
MPU->RLAR = 0x2001FFFF | MPU_RLAR_EN_Msk;  // limit + enable
MPU->CTRL = MPU_CTRL_ENABLE_Msk | MPU_CTRL_PRIVDEFENA_Msk;
```

---

## 下一步

→ [Module 4：記憶體保護（MMU / MPU）](04-memory-protection.md)
