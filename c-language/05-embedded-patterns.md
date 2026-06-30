---
date: 2026-06-15
type: tech-note
tags: [c-language, embedded, module]
output: study
status: complete
---

> [!abstract] TL;DR
> `volatile`、memory-mapped struct、`__attribute__` 修飾符是嵌入式 C 的關鍵語法。`volatile` 防止編譯器快取硬體暫存器值；`packed`/`aligned` 控制記憶體佈局；`static inline` 提供零開銷工具函式。

# C 語言 Module 5：嵌入式關鍵語法

## `volatile`

### 為什麼需要

編譯器會對程式碼做最佳化，例如：

```c
uint32_t status = *reg;
while (status == 0) {
    status = *reg;  // 編譯器可能把這行刪掉，因為「status 沒有被改過」
}
```

但硬體可能在任何時刻改變暫存器的值。加 `volatile` 告訴編譯器：  
**「這個值可能被外部改變，每次都要重新讀，不要快取在暫存器裡。」**

```c
volatile uint32_t *status_reg = (volatile uint32_t *)0x50000010;

while (*status_reg == 0) {
    // 每次迴圈都會真的去讀硬體暫存器
}
```

**Volatile 效果對比：**

```
不加 volatile（編譯器最佳化後）：
  status = *reg;           ← 只讀一次，存進 CPU 暫存器
  while (status == 0) {    ← 一直用 CPU 暫存器舊值，永遠迴圈
  }
  硬體改了 *reg → 程式看不到 ✗

加 volatile：
  while (*reg == 0) {      ← 每次都去記憶體讀最新值
  }
  硬體改了 *reg → 下一次迴圈讀到 → 迴圈結束 ✓
```

### 何時加 `volatile`

**核心原則：任何不是由當前執行緒的程式碼修改的變數。**

| 情況 | 說明 |
|------|------|
| 硬體暫存器 | 硬體直接改記憶體位址的值 |
| ISR（中斷服務程式） | 硬體觸發 CPU 跳去執行，再改變數 |
| DMA | 獨立硬體單元直接搬運記憶體，不經過 CPU |
| 多核心共用記憶體 | 另一顆核心（如 A35）寫入，M33 讀取 |

前三種都是「硬體觸發、不在編譯器掌控內的變動」，本質相同。

**ISR 說明：**

硬體發生事件（按鈕、計時器、資料到達）時，CPU 暫停目前程式，跳去執行 ISR，處理完再跳回：

```
main() 執行中...
    ↓
[按鈕按下] ← 硬體中斷信號
    ↓
CPU 暫停 main，執行 gpio_irq_handler()
    ↓
ISR 執行完，CPU 跳回 main 繼續
```

**DMA 說明：**

CPU 不需要自己搬資料，DMA 在背景直接寫入記憶體：

```
CPU：「DMA，把 UART 資料搬到 buf」→ CPU 繼續做其他事
DMA：默默把資料寫進 buf（CPU 不知道）
CPU：回來讀 buf → 若沒有 volatile，可能讀到快取的舊值
```

```c
// ISR 修改，main loop 讀取
volatile uint32_t irq_count = 0;

void gpio_irq_handler(void) {
    irq_count++;  // ISR 裡修改
}

void main_loop(void) {
    while (irq_count < 10) {
        // 若沒有 volatile，編譯器可能把 irq_count 快取，永遠迴圈
    }
}
```

---

## Memory-Mapped I/O 模式

### 直接位址存取

```c
volatile uint32_t *reg = (volatile uint32_t *)0x50003000;
*reg = 0x1;
```

### Struct 對應暫存器群組（更常見）

```c
// 把一組相鄰的暫存器定義成 struct
typedef struct {
    volatile uint32_t MODER;   // offset 0x00
    volatile uint32_t OTYPER;  // offset 0x04
    volatile uint32_t OSPEEDR; // offset 0x08
    volatile uint32_t PUPDR;   // offset 0x0C
    volatile uint32_t IDR;     // offset 0x10
    volatile uint32_t ODR;     // offset 0x14
} GPIO_TypeDef;

// 把硬體位址轉成 struct 指標
#define GPIOA  ((GPIO_TypeDef *)0x50002000)

// 直接用欄位名稱操作暫存器
GPIOA->MODER |= (1 << 10);
GPIOA->ODR   |= (1 << 5);
```

**記憶體位址空間示意（STM32MP2）：**

```
位址空間：
0xFFFFFFFF ┌────────────────────┐
           │                    │
0x50003000 ├────────────────────┤ ← GPIOB_ODR
0x50002000 ├────────────────────┤ ← GPIOA（MODER, ODR...）
           │  Peripheral Regs   │   寫入 = 控制硬體（硬體暫存器區域）
0x40000000 ├────────────────────┤
           │                    │
0x20040000 ├────────────────────┤ ← RAM 頂部
           │       SRAM         │   你的變數、stack 在這
0x20000000 ├────────────────────┤ ← RAM 開始
           │                    │
0x08200000 ├────────────────────┤ ← Flash 頂部
           │    NOR Flash       │   程式碼、常數在這
0x08000000 ├────────────────────┤ ← Flash 開始
```

這就是 ST 的 HAL library 的設計方式，整個 STM32 HAL 都是這個模式。

---

## Preprocessor Macro

### `#define` 常數

```c
#define FLASH_BASE_ADDR  0x08000000UL
#define SECTOR_SIZE      0x1000U        // 4KB

// 計算 sector 位址
uint32_t sector_addr = FLASH_BASE_ADDR + (sector_num * SECTOR_SIZE);
```

`UL` 後綴：unsigned long，避免在 32-bit 系統上整數溢位。

### `#define` 函式型 Macro

```c
// 簡單的 macro（沒有型別檢查）
#define MAX(a, b)  ((a) > (b) ? (a) : (b))
#define ABS(x)     ((x) < 0 ? -(x) : (x))

// 括號很重要！
// 沒括號：MAX(1+2, 3) → 1+2 > 3 ? 1+2 : 3  → 可能出錯
// 有括號：MAX(1+2, 3) → (1+2) > (3) ? (1+2) : (3)  → 正確
```

### 條件編譯

```c
// 除錯模式
#ifdef DEBUG
    #define LOG(msg) printf("[DEBUG] %s\n", msg)
#else
    #define LOG(msg)  // release 模式時展開成空的，零開銷
#endif

// 防止重複引入（每個 .h 都要有）
#ifndef MY_MODULE_H
#define MY_MODULE_H
// ... 內容 ...
#endif
```

### 字串化與連接

```c
#define STRINGIFY(x)  #x           // 把 x 變成字串
#define CONCAT(a, b)  a ## b       // 連接兩個 token

STRINGIFY(hello)     // → "hello"
CONCAT(gpio, _init)  // → gpio_init（函式名）
```

---

## `__attribute__` 修飾符（GCC/Clang）

嵌入式常用，告訴編譯器特殊處理：

```c
// packed：不加 padding，struct 完全緊密排列
// 用於 protocol 封包、flash 資料格式
typedef struct __attribute__((packed)) {
    uint8_t  version;   // offset 0
    uint16_t length;    // offset 1（通常會被 padding 到 offset 2）
    uint32_t checksum;  // offset 3
} FirmwareHeader;
```

**Padding（填充）是什麼：**

CPU 讀取 `uint32_t` 時，從「4 的倍數」位址讀最快。編譯器為了讓每個欄位對齊，會自動插入空白 bytes：

```
不加 packed：
offset: 0    1    2    3    4    5    6    7
       [ver][pad][pad][pad][   checksum    ]
              ↑
        編譯器自動補 3 bytes，讓 checksum 對齊到 offset 4

加了 packed：
offset: 0    1    2    3    4
       [ver][ length  ][  checksum  ]
        緊密排列，沒有空隙
```

**取捨：**

| | 不加 packed | 加 packed |
|--|------------|----------|
| 速度 | 快（對齊，CPU 一次讀完）| 慢一點（可能需要多次讀）|
| 用途 | 一般計算用的 struct | flash 格式、網路封包（需要精確控制 byte 位置）|

RoT 只在「解析 flash header」時用 packed，核心邏輯的 struct 不加。

### `section`：指定放在哪個記憶體段

程式編譯後會被分成幾個「段（section）」放進 Flash：`.text`（程式碼）、`.data`（已初始化變數）、`.rodata`（唯讀常數）。`section` 讓你強制把某個變數或函式放在指定的段。

最常見的用途是向量表，ARM 規定向量表必須在 Flash 最前面的固定位址，否則開機時 CPU 找不到：

```c
__attribute__((section(".vectors")))
uint32_t vector_table[] = {
    (uint32_t)&_stack_top,       // 初始 Stack Pointer
    (uint32_t)reset_handler,     // 開機跳這裡
    (uint32_t)default_irq_handler,
    // ...
};
```

Linker script（`.ld` 檔）再把 `.vectors` 段放到 `0x00000000`，確保位址正確。

### `weak`：弱符號（可被覆蓋的預設實作）

韌體向量表需要填入幾十到幾百個 IRQ handler 的位址，但你通常只會實作其中幾個。沒有 `weak` 的話，未實作的 handler 連結器會報「找不到定義」。

`weak` 讓你提供一個預設實作，如果外部有同名的「強符號」，連結器自動用強符號覆蓋：

```c
// startup.c — 預設：卡在 while(1)，表示發生了未處理的中斷
__attribute__((weak))
void default_irq_handler(void) {
    while(1);
}

// 你只需要寫你關心的，其他全用預設
void uart_irq_handler(void) {   // 強符號，覆蓋 weak
    // 處理 UART 中斷
}
```

```
連結器選擇規則：
uart_irq_handler → 你寫了 → 用你的版本 ✓
gpio_irq_handler → 沒寫   → 用 weak 預設版本（while(1)）✓
spi_irq_handler  → 沒寫   → 用 weak 預設版本（while(1)）✓
```

C# 類比：類似 `virtual` 方法，基底類別給預設實作，子類別選擇性 `override`。

### `aligned`：對齊到指定 byte 邊界

強制讓變數的起始位址是指定數字的倍數：

```c
uint8_t dma_buffer[256] __attribute__((aligned(32)));
// dma_buffer 起始位址保證是 32 的倍數（如 0x20000020、0x20000040）
```

**為什麼 DMA 需要對齊：**

DMA controller 是獨立的硬體單元，它搬資料時直接操作記憶體匯流排。硬體規格要求 buffer 起始位址必須對齊，否則：

```
沒有 aligned：
  dma_buffer 可能在 0x20000003（不是 32 的倍數）
  DMA 硬體拒絕這個位址，或搬到錯誤位置，或觸發 HardFault ✗

有 aligned(32)：
  dma_buffer 保證在 0x20000020（32 的倍數）
  DMA 硬體接受，正常運作 ✓
```

### `noreturn`：告訴編譯器這個函式不會返回

某些函式執行後永遠不會回到呼叫者，例如進入錯誤停止狀態：

```c
__attribute__((noreturn))
void system_halt(void) {
    while(1);
}
```

**為什麼要告訴編譯器：**

編譯器不知道 `while(1)` 永不結束，會在函式後面產生多餘的清理指令（pop registers、return），浪費 Flash 空間。加了 `noreturn` 後編譯器知道不需要這些指令，也能在呼叫後做更積極的最佳化：

```
不加 noreturn：
  system_halt() 呼叫後，編譯器仍產生後續程式碼（雖然永遠不會執行）

加了 noreturn：
  編譯器知道 system_halt() 後面的程式碼不可能執行
  → 不產生多餘指令，節省 Flash ✓
  → 若你在 noreturn 函式後還有程式碼，編譯器會發出警告 ✓
```

---

## `inline` 函式 vs Macro

嵌入式常面臨選擇：

```c
// Macro：無型別檢查，展開在呼叫處，可能有副作用
#define SQUARE(x) ((x) * (x))
SQUARE(i++)  // i 被遞增兩次！

// inline 函式：有型別檢查，通常也會展開（零呼叫開銷）
static inline uint32_t square(uint32_t x) {
    return x * x;
}
square(i++)  // i 只遞增一次，安全
```

**規則**：有型別的操作用 `static inline`，純文字替換才用 `#define`。

### 為什麼實務上 `static inline` 放在 `.h`

嵌入式專案裡，短小的工具函式（讀暫存器、bit 操作）常常放在 `.h`，讓所有 `.c` 都能用到且零開銷：

```c
// utils.h
static inline uint32_t bit_get(uint32_t reg, uint8_t n) {
    return (reg >> n) & 1;
}
```

**`static` 的作用**：`.h` 會被多個 `.c` include，前處理器把內容貼進每個 `.c`，若沒有 `static`，連結時會報「重複定義」：

```
main.o  有一份 bit_get 定義
flash.o 有一份 bit_get 定義   → 連結器報錯 ✗
uart.o  有一份 bit_get 定義

加了 static：每份都是私有的，各自獨立，連結器不衝突 ✓
```

**`inline` 的作用**：提示編譯器把函式展開在呼叫處，不產生跳轉，零開銷。開 `-O2` 時編譯器會自動判斷要不要展開（即使你沒寫 `inline` 也會展開短函式）。

**不適合放 `.h` 的情況**：
- 函式很長（展開多次會讓 Flash 膨脹）
- 遞迴函式（無法展開）
- 複雜邏輯（如 `verify_firmware`）→ 放 `.c`，透過 `.h` 宣告對外公開

---

## Designated Initializer（C99）

初始化 struct 時指定欄位名稱，避免順序錯誤：

```c
typedef struct {
    uint32_t base_addr;
    uint32_t size;
    uint8_t  read_only;
} MemoryRegion;

// 舊方式（順序錯了就出事）
MemoryRegion r = {0x08000000, 0x10000, 1};

// C99 Designated Initializer（推薦）
MemoryRegion r = {
    .base_addr = 0x08000000,
    .size      = 0x10000,
    .read_only = 1,
};
```

嵌入式程式碼幾乎全用這個寫法。

---

## 下一步

> [!tip] 複習問題
> 1. 為什麼硬體暫存器指標要加 `volatile`？如果不加，CPU 可能讀到什麼？
> 2. `__attribute__((packed))` 的代價是什麼？RoT 專案中什麼情況才使用？
> 3. `static inline` 函式放在 `.h` 時，`static` 的作用是什麼？沒有 `static` 會發生什麼連結錯誤？

→ [[06-memory-model|Module 6：記憶體模型]]
