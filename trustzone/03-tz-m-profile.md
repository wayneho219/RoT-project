---
tags: [trustzone, module]
topic: trustzone
week: "5-6"
---
# TrustZone Module 3：M-profile TrustZone（M33 側）

## 概覽

M33 的 TrustZone 和 A35 不同：

| | A35（ARMv8-A）| M33（ARMv8-M）|
|-|--------------|--------------|
| 隔離粒度 | 頁面（4 KB）+ TZASC 區域 | SAU Region（任意對齊）|
| 世界切換 | SMC（software trap）| BXNS / BLXNS（函式呼叫）|
| Secure Monitor | BL31 常駐 EL3 | 無，Secure 直接控制 NS |
| 切換開銷 | 高（context save/restore）| 低（register 自動清除）|

---

## SAU（Security Attribution Unit）詳解

SAU 最多定義 **8 個 region**，每個 region 可以是：
- `Secure`（NSCALL = 0）
- `Non-Secure Callable`（NSCALL = 1）：NS 可以呼叫進入的入口
- 若記憶體不在任何 SAU region 內：視 SAU->CTRL.ALLNS 而定

```c
// SAU 配置範例（開機時在 M33 Reset_Handler 中設定）

// 先停用 SAU，初始化完成後再開啟
SAU->CTRL = 0;

// Region 0：Secure Code Flash（只有 Secure 可讀執行）
SAU->RNR  = 0;
SAU->RBAR = 0x0E000000;                          // Base（32-byte 對齊）
SAU->RLAR = (0x0E07FFFF & 0xFFFFFFE0)            // Limit（必須 -1）
          | SAU_RLAR_ENABLE_Msk;                  // Enable，NSCALL=0 → Secure

// Region 1：Non-Secure Callable（NS 的入口大廳）
// 存放 __attribute__((cmse_nonsecure_entry)) 函式
SAU->RNR  = 1;
SAU->RBAR = 0x0E07E000;
SAU->RLAR = (0x0E07FFFF & 0xFFFFFFE0)
          | SAU_RLAR_NSC_Msk                      // NSC = 1
          | SAU_RLAR_ENABLE_Msk;

// Region 2：Secure SRAM
SAU->RNR  = 2;
SAU->RBAR = 0x20000000;
SAU->RLAR = (0x2001FFFF & 0xFFFFFFE0)
          | SAU_RLAR_ENABLE_Msk;

// Region 3：Non-Secure SRAM（M33 和 A35 共享通訊區）
SAU->RNR  = 3;
SAU->RBAR = 0x20020000;
SAU->RLAR = (0x2003FFFF & 0xFFFFFFE0);           // ENABLE=0 or NS
// 注意：不在任何 Secure SAU region 內 → 視為 NS

// 啟用 SAU
SAU->CTRL = SAU_CTRL_ENABLE_Msk;
__DSB();
__ISB();
```

---

## NSC（Non-Secure Callable）詳解

### 為什麼需要 NSC

Non-Secure 程式碼不能直接跳到 Secure code（SecureFault）。  
NSC 是 Secure code 中一個特殊的「入口區域」，NS 只能跳進 NSC 裡的函式。

```
NS code 試圖跳到 Secure code：
  ├── 跳到 NSC region 的入口點 → 合法，世界切換
  └── 跳到其他 Secure 位址 → SecureFault（硬體擋住）
```

### 定義 Secure Entry Function

```c
// 在 Secure side 的 gateway.c

// __attribute__((cmse_nonsecure_entry)) 讓編譯器：
// 1. 把這個函式放在 .gnu.sgstubs section（NSC 區域）
// 2. 在入口插入 SG（Secure Gateway）指令
// 3. 函式返回前清除 Secure 側的暫存器（防洩漏）

__attribute__((cmse_nonsecure_entry))
uint32_t secure_fw_version_get(void) {
    return bsec_read_fw_version();  // 從 OTP 讀取
}

__attribute__((cmse_nonsecure_entry))
int32_t secure_verify_hash(const uint8_t *ns_data, uint32_t len,
                           uint8_t *ns_output) {
    // 注意：ns_data 是 NS 的指標，必須驗證！
    // 用 cmse_check_pointed_object 確認指標是 NS 的
    if (cmse_check_address_range((void*)ns_data, len, CMSE_NONSECURE) == NULL) {
        return -1;  // NS 試圖傳 Secure 指標進來 → 拒絕
    }

    // 把資料複製到 Secure SRAM 後再處理（防 TOCTOU）
    static uint8_t secure_buf[MAX_FW_SIZE];
    if (len > MAX_FW_SIZE) return -2;
    memcpy(secure_buf, ns_data, len);

    sha256(secure_buf, len, ns_output);
    return 0;
}
```

### Non-Secure 側呼叫

```c
// 在 Non-Secure 側（例如 A35 Linux 的 M33 driver，或 M33 的 NS partition）

// 1. 從 Secure 側的 veneer table 取得函式指標
typedef uint32_t (*secure_fw_version_get_t)(void) __attribute__((cmse_nonsecure_call));
#define SECURE_GATEWAY_BASE 0x0E07E000  // NSC 區域基底

// 2. 呼叫（透過 BLXNS 指令）
secure_fw_version_get_t get_version = 
    (secure_fw_version_get_t)(SECURE_GATEWAY_BASE + VERSION_OFFSET);
uint32_t ver = get_version();
```

---

## 安全函式返回時的暫存器清除

GCC 的 `cmse_nonsecure_entry` 屬性確保返回前清除 Secure 暫存器：

```asm
; 編譯器產生的 NSE function 返回 prologue
PUSH    {r4-r11, lr}          ; 保存 Secure callee-saved regs
; ... 函式本體 ...
POP     {r4-r11}              ; 恢復 callee-saved regs
; 清除 r1-r3（參數和暫時暫存器）
MOV     r1, #0
MOV     r2, #0  
MOV     r3, #0
; 清除 ip, lr（不讓 NS 看到 Secure 位址）
MOV     ip, #0
BXNS    lr                    ; 返回 Non-Secure（LSB = 0 表示 NS 位址）
```

---

## Secure Fault 處理

當 NS 試圖存取 Secure 記憶體時，觸發 SecureFault：

```c
// SecureFault handler（Secure 側）
void SecureFault_Handler(void) {
    // 讀取 SFSR（Secure Fault Status Register）了解原因
    uint32_t sfsr = SAU->SFSR;

    if (sfsr & SAU_SFSR_INVEP_Msk)  // Invalid Entry Point
        fault_reason = "NS 試圖跳到非 NSC 的 Secure 位址";
    if (sfsr & SAU_SFSR_AUVIOL_Msk) // Attribution Unit Violation
        fault_reason = "NS 存取了 Secure 記憶體";
    if (sfsr & SAU_SFSR_INVTRAN_Msk)// Invalid Transition
        fault_reason = "跳到 Secure 但沒用 SG 指令";

    // 記錄並鎖死（安全場景不能繼續執行）
    error_log_write(fault_reason);
    while (1);
}
```

---

## 下一步

→ [Module 4：Secure Storage](04-secure-storage.md)
