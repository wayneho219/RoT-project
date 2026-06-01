---
type: concept
title: C 函式指標與回調模式
tags: [C語言, 函式指標, callback, 嵌入式, 設計模式]
sources: [c-language-學習筆記]
created: 2026-05-25
updated: 2026-05-25
---

# C 函式指標與回調模式

## 一句話定義

函式在記憶體中有位址，函式指標存這個位址；C# 對應是 `delegate`/`Action`/`Func`，在嵌入式用於 callback、jump table、模擬 interface。

## 基本語法

```c
int (*op)(int, int);    // 宣告：回傳 int，兩個 int 參數
op = add;               // 賦值（函式名稱本身就是位址）
int result = op(3, 4);  // 呼叫
```

語法記憶：`回傳型別 (*指標名)(參數型別)`

`typedef` 簡化：
```c
typedef int (*BinaryOp)(int, int);
BinaryOp op = add;
```

## 三個嵌入式核心用途

### 1. Callback（回調）

事件發生時呼叫使用者指定函式，不需要 if-else 硬編碼：

```c
typedef void (*IrqHandler)(void);
void irq_register(IrqEntry *e, uint32_t num, IrqHandler h);
// 使用
irq_register(&gpio_irq, 5, my_gpio_handler);
```

### 2. Jump Table（分派表）

根據 ID 直接跳到對應函式，比 switch-case 乾淨：

```c
Command dispatch_table[] = { cmd_reset, cmd_status, cmd_verify };
dispatch_table[cmd_id]();
```

RoT 的指令處理幾乎都是這個模式。

### 3. 模擬 Interface（C 的 interface）

```c
typedef struct {
    int  (*read)(uint8_t *buf, uint32_t len);
    int  (*write)(const uint8_t *buf, uint32_t len);
    void (*reset)(void);
} FlashDriver;

FlashDriver nor_driver = { .read=nor_read, .write=nor_write, .reset=nor_reset };
verify_firmware(&nor_driver);  // 不需要知道是哪種 flash
```

等同 C# 的 `interface`，讓驗證邏輯可以換不同儲存媒介。

## ARM 向量表

ARM Cortex-M 的中斷向量表本質是函式指標陣列：

```c
typedef void (*VectorEntry)(void);
__attribute__((section(".vectors")))
VectorEntry vector_table[] = {
    (VectorEntry)0x20010000,  // 初始 Stack Pointer
    reset_handler,             // Reset → M33 開機後第一個執行
    nmi_handler,
    hardfault_handler,
};
```

## 常見錯誤

```c
// 呼叫 NULL 函式指標 → crash
if (cb != NULL) cb();   // 正確：使用前檢查

// 型別不符（compiler 不一定報錯）
void handler(uint32_t arg) {}
typedef void (*Handler)(void);
Handler h = handler;    // UB：參數型別不匹配
```

## 相關頁面

- [[c-基礎與型別系統]] — C# interface 的 C 對應
- [[c-未定義行為與安全程式碼]] — NULL 指標問題
