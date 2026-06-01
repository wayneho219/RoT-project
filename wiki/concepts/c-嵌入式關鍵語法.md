---
type: concept
title: C 嵌入式關鍵語法
tags: [C語言, volatile, macro, MMIO, 嵌入式]
sources: [c-language-學習筆記]
created: 2026-05-25
updated: 2026-05-25
---

# C 嵌入式關鍵語法

## 一句話定義

嵌入式 C 需要三個通用 C 不常用的工具：`volatile`（防止編譯器最佳化硬體存取）、Preprocessor Macro（零開銷的常數與條件編譯）、`__attribute__`（記憶體佈局控制）。

## `volatile`

### 為什麼需要

沒有 `volatile`，編譯器可能把「看起來沒有改變的值」最佳化掉：

```c
// 沒有 volatile：編譯器可能把迴圈最佳化成無窮迴圈（認為 status 永遠是 0）
while (*status_reg == 0) {}

// 加 volatile：每次迴圈都真的讀硬體暫存器
volatile uint32_t *status_reg = (volatile uint32_t *)0x50000010;
```

### 何時加

- Memory-Mapped 硬體暫存器
- 被 ISR（中斷服務程式）修改的變數
- DMA 或多核心共用的記憶體

## Memory-Mapped I/O：Struct 模式

把相鄰暫存器群組定義為 struct（ST HAL Library 的設計方式）：

```c
typedef struct {
    volatile uint32_t MODER;    // offset 0x00
    volatile uint32_t OTYPER;   // offset 0x04
    volatile uint32_t OSPEEDR;  // offset 0x08
    volatile uint32_t ODR;      // offset 0x14
} GPIO_TypeDef;

#define GPIOA  ((GPIO_TypeDef *)0x50002000)

GPIOA->MODER |= (1 << 10);  // 比直接算偏移位址清楚得多
```

## Preprocessor Macro

### 函式型 Macro（括號很重要）

```c
#define MAX(a, b)  ((a) > (b) ? (a) : (b))  // 每個參數都要括號
```

### 條件編譯

```c
#ifdef DEBUG
    #define LOG(msg) printf("[DEBUG] %s\n", msg)
#else
    #define LOG(msg)   // release 模式展開成空的，零開銷
#endif
```

### `static inline` vs `#define`

| | `#define` | `static inline` |
|--|-----------|-----------------|
| 型別檢查 | 無 | 有 |
| 副作用 | 可能（`i++` 執行兩次）| 無 |
| 適用 | 純文字替換 | 有型別的操作 |

**規則**：有型別的操作用 `static inline`，純文字替換才用 `#define`。

## `__attribute__`（GCC/Clang）

```c
// packed：不加 padding（protocol 封包、flash 資料格式）
typedef struct __attribute__((packed)) {
    uint8_t  version;
    uint16_t length;
} FirmwareHeader;

// section：指定放哪個記憶體段
__attribute__((section(".vectors"))) uint32_t vector_table[];

// weak：弱符號，可被同名強符號覆蓋（提供預設實作）
__attribute__((weak)) void default_irq_handler(void) { while(1); }

// aligned：對齊邊界（DMA buffer 通常需要 32-byte 對齊）
uint8_t dma_buffer[256] __attribute__((aligned(32)));
```

## Designated Initializer（C99）

初始化 struct 時指定欄位名稱，避免順序錯誤：

```c
MemoryRegion r = {
    .base_addr = 0x08000000,
    .size      = 0x10000,
    .read_only = 1,
};
```

嵌入式程式碼幾乎全用這個寫法。

## 相關頁面

- [[c-指標與記憶體存取]] — volatile 指標的基礎
- [[c-位元運算]] — 搭配 MMIO 的暫存器操作
- [[c-記憶體模型與分段]] — `.section` 與 linker 的關係
