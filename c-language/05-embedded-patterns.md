---
tags: [c-language, module]
topic: c-language
week: "1-2"
---
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

### 何時加 `volatile`

- Memory-mapped 硬體暫存器
- 被中斷服務程式（ISR）修改的變數
- 多核心或 DMA 共用的記憶體

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

// section：指定放在哪個記憶體段
// 向量表必須在特定位址
__attribute__((section(".vectors")))
uint32_t vector_table[];

// weak：弱符號，可以被同名的強符號覆蓋
// 用於提供預設實作，讓使用者可以選擇性覆蓋
__attribute__((weak))
void default_irq_handler(void) {
    while(1);  // 預設：停住
}

// aligned：對齊到指定 byte 邊界
// DMA buffer 通常需要 32-byte 對齊
uint8_t dma_buffer[256] __attribute__((aligned(32)));

// noreturn：告訴編譯器這個函式不會返回
__attribute__((noreturn))
void system_halt(void) {
    while(1);
}
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

→ [Module 6：記憶體模型](06-memory-model.md)
