---
tags: [stm32mp2, module]
topic: stm32mp2
week: "9+"
---
# STM32MP2 Module 3：M33 TD 初始化

## M33 TD 在 STM32MP215F 的地位

```
上電後的 boot 順序：
  1. ROM Code（M33 執行）→ 從 NOR Flash 載入 M33 firmware
  2. M33 Secure Firmware 執行（這是我們要寫的）
  3. M33 hold A35 in reset
  4. 驗證完成 → release A35

M33 TD（Trusted Domain）的硬體特性：
  ├── 直接存取 BSEC/OTP（不需要 Secure Monitor）
  ├── 控制 A35 reset 腳（透過 RCC）
  ├── 擁有 Secure SRAM（A35 看不到）
  └── 可設定 ETZPC（每個 peripheral 的安全屬性）
```

---

## M33 Startup Code（startup.s）

```asm
; startup.s（精簡版）
    .syntax unified
    .cpu cortex-m33
    .thumb

    .section .isr_vector,"a",%progbits
    .global _isr_vector
_isr_vector:
    .word _estack               ; 0x00: Initial MSP（Stack Pointer）
    .word Reset_Handler         ; 0x04: Reset
    .word NMI_Handler           ; 0x08
    .word HardFault_Handler     ; 0x0C
    .word MemManage_Handler     ; 0x10
    .word BusFault_Handler      ; 0x14
    .word UsageFault_Handler    ; 0x18
    .word SecureFault_Handler   ; 0x1C  ← ARMv8-M TrustZone 新增
    ; ... 其他中斷向量 ...

    .text
    .thumb_func
    .global Reset_Handler
Reset_Handler:
    ; 載入初始 Stack Pointer（已由向量表第一個 word 設定）
    
    ; 複製 .data 段到 RAM
    ldr  r0, =_sdata
    ldr  r1, =_edata
    ldr  r2, =_sidata      ; .data 在 ROM 的位置
copy_data:
    cmp  r0, r1
    bge  zero_bss
    ldr  r3, [r2], #4
    str  r3, [r0], #4
    b    copy_data

    ; 清零 .bss 段
zero_bss:
    ldr  r0, =_sbss
    ldr  r1, =_ebss
    mov  r2, #0
clear_bss:
    cmp  r0, r1
    bge  call_main
    str  r2, [r0], #4
    b    clear_bss

call_main:
    bl   SystemInit    ; 時鐘初始化
    bl   main
    
infinite_loop:
    b    infinite_loop  ; 不應該到這裡
```

---

## SystemInit（時鐘配置）

```c
// system_stm32mp2xx.c

void SystemInit(void) {
    // 1. 啟用 FPU（Floating Point Unit）
    SCB->CPACR |= (0xF << 20);  // CP10 / CP11 全開
    __DSB();
    __ISB();

    // 2. 配置 HSI（High Speed Internal，64 MHz）作為系統時鐘
    RCC->HSICFGR = (0 << RCC_HSICFGR_HSITRIM_Pos);  // 預設 trim
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY));  // 等 HSI 穩定

    // 3. 對 M33 來說，64 MHz 夠用了（A35 用 PLL 跑更快）
    
    // 4. 啟用所需周邊的時鐘
    RCC->AHB2ENR |= RCC_AHB2ENR_HASHEN;  // HASH 加速器
    RCC->AHB2ENR |= RCC_AHB2ENR_PKACEN;  // PKA 加速器
    RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;   // RNG
    RCC->APB4ENR |= RCC_APB4ENR_BSECEN;  // BSEC
    __DSB();
}
```

---

## SAU 初始化

```c
// sau_init() 在 main() 最前面呼叫

void sau_init(void) {
    // 先停用 SAU
    SAU->CTRL = 0;

    // Region 0: Secure Code Flash（0x0E000000 – 0x0E07DFFF）
    SAU->RNR  = 0;
    SAU->RBAR = 0x0E000000;
    SAU->RLAR = (0x0E07DFFF & SAU_RLAR_LADDR_Msk) | SAU_RLAR_ENABLE_Msk;

    // Region 1: NSC Gate（0x0E07E000 – 0x0E07FFFF，Non-Secure Callable）
    SAU->RNR  = 1;
    SAU->RBAR = 0x0E07E000;
    SAU->RLAR = (0x0E07FFFF & SAU_RLAR_LADDR_Msk) 
              | SAU_RLAR_NSC_Msk 
              | SAU_RLAR_ENABLE_Msk;

    // Region 2: Secure SRAM（0x20000000 – 0x2001FFFF）
    SAU->RNR  = 2;
    SAU->RBAR = 0x20000000;
    SAU->RLAR = (0x2001FFFF & SAU_RLAR_LADDR_Msk) | SAU_RLAR_ENABLE_Msk;

    // Region 3: Secure Peripheral（BSEC、RCC secure registers）
    SAU->RNR  = 3;
    SAU->RBAR = 0x50000000;
    SAU->RLAR = (0x5FFFFFFF & SAU_RLAR_LADDR_Msk) | SAU_RLAR_ENABLE_Msk;

    // 啟用 SAU（未覆蓋的區域視為 Non-Secure）
    SAU->CTRL = SAU_CTRL_ENABLE_Msk;
    __DSB();
    __ISB();
}
```

---

## M33 ↔ A35 通訊（共享記憶體 IPC）

```c
// 共享記憶體佈局（NS SRAM，A35 和 M33 都能讀寫）
#define IPC_BASE  0x20020000

typedef struct {
    volatile uint32_t magic;        // 0x524F5449 = "ROTI"
    volatile uint32_t version;      // IPC 協議版本
    volatile uint32_t m33_status;   // M33 的狀態（驗證結果）
    volatile uint32_t fw_version;   // 已驗證的 firmware 版本
    volatile uint8_t  reserved[48]; // 預留
} IPC_Header;

#define M33_STATUS_VERIFYING  0x00000001
#define M33_STATUS_OK         0x00000002
#define M33_STATUS_FAIL       0x000000FF

IPC_Header *ipc = (IPC_Header *)IPC_BASE;

// M33 設定初始狀態
void ipc_init(void) {
    ipc->magic     = 0x524F5449;
    ipc->version   = 1;
    ipc->m33_status = M33_STATUS_VERIFYING;
    ipc->fw_version = 0;
    __DSB();   // 確保 A35 看到更新
}

// A35 啟動後檢查這個區域
// Linux driver: /drivers/rot/rot_ipc.c
// 讀取 ipc->m33_status，確認是 M33_STATUS_OK
```

---

## ETZPC 初始化

```c
// 設定各 peripheral 的安全屬性
void etzpc_init(void) {
    ETZPC_HandleTypeDef hetzpc;
    hetzpc.Instance = ETZPC;
    HAL_ETZPC_Init(&hetzpc);

    // Secure-only：加密加速器
    HAL_ETZPC_Set_SAES_PeriphProtection(&hetzpc, ETZPC_PERIPH_PROTECTION_READ_WRITE_SECURE);
    HAL_ETZPC_Set_PKA_PeriphProtection(&hetzpc,  ETZPC_PERIPH_PROTECTION_READ_WRITE_SECURE);
    HAL_ETZPC_Set_RNG_PeriphProtection(&hetzpc,  ETZPC_PERIPH_PROTECTION_READ_WRITE_SECURE);
    HAL_ETZPC_Set_BSEC_PeriphProtection(&hetzpc, ETZPC_PERIPH_PROTECTION_READ_WRITE_SECURE);

    // Non-Secure：A35 可用的 peripheral
    HAL_ETZPC_Set_USART2_PeriphProtection(&hetzpc, ETZPC_PERIPH_PROTECTION_READ_WRITE_NONSECURE);
    HAL_ETZPC_Set_I2C1_PeriphProtection(&hetzpc,   ETZPC_PERIPH_PROTECTION_READ_WRITE_NONSECURE);
}
```

---

## 完整 main() 骨架

```c
int main(void) {
    // 1. 硬體初始化
    SystemInit();       // 時鐘
    sau_init();         // TrustZone 記憶體隔離
    etzpc_init();       // Peripheral 安全屬性
    mpu_init();         // 記憶體保護
    ipc_init();         // 共享記憶體初始化

    // 2. 持有 A35 in reset（確保驗證完成前 A35 不啟動）
    __HAL_RCC_A35_FORCE_RESET();

    // 3. 初始化加密相關
    HAL_RNG_Init(&hrng);
    HAL_HASH_Init(&hhash);
    HAL_PKA_Init(&hpka);

    // 4. 從 BSEC 讀取安全設定
    uint8_t rotpkh[16];
    bsec_read_rotpkh(rotpkh);

    // 5. 讀取並驗證 A35 firmware
    int ret = verify_a35_firmware(rotpkh);

    if (ret == 0) {
        // 6a. 驗證通過 → release A35
        ipc->m33_status = M33_STATUS_OK;
        ipc->fw_version = firmware_header.version;
        __DSB();
        __HAL_RCC_A35_RELEASE_RESET();
    } else {
        // 6b. 驗證失敗 → 鎖死
        ipc->m33_status = M33_STATUS_FAIL;
        __DSB();
        while (1);  // A35 永遠不啟動
    }

    // 7. M33 進入監控迴圈（可選：監控 A35 狀態、回應 Secure API 呼叫）
    while (1) {
        __WFI();  // 等待中斷（省電）
    }
}
```

---

## 下一步

→ [Module 4：完整 RoT 實作](04-rot-implementation.md)
