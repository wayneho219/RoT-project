---
date: 2026-06-15
type: tech-note
tags: [c-language, embedded, module]
output: study
status: complete
---

> [!abstract] TL;DR
> 嵌入式最常用技能。硬體暫存器的每個 bit 都有語意，必須精準 set/clear/toggle/read 任意 bit 而不影響其他 bit。四個操作模式加上 `BIT(n)` macro 是嵌入式標準寫法，STM32 GPIO 設定是最典型應用。

# C 語言 Module 3：Bit Manipulation

嵌入式最常用的技能。硬體暫存器的每個 bit 都有意義，你必須能精準地設定、清除、讀取任意一個 bit，而不影響其他 bit。

---

## 六個基本運算子

| 運算子 | 名稱 | 範例 |
|--------|------|------|
| `&` | AND（位元且）| `0b1100 & 0b1010 = 0b1000` |
| `\|` | OR（位元或） | `0b1100 \| 0b1010 = 0b1110` |
| `^` | XOR（互斥或）| `0b1100 ^ 0b1010 = 0b0110` |
| `~` | NOT（位元反轉）| `~0b1100 = 0b0011`（全部翻轉）|
| `<<` | 左移 | `0b0001 << 3 = 0b1000` |
| `>>` | 右移 | `0b1000 >> 2 = 0b0010` |

---

## 四個核心操作

### 設定 bit（Set）：讓某個 bit 變 1

```c
uint32_t reg = 0b00000000;

// 設定 bit 3（從 0 開始數）
reg = reg | (1 << 3);   // 0b00001000
// 簡寫：
reg |= (1 << 3);
```

原理：`1 << 3` 產生 mask `0b00001000`，OR 之後只有 bit 3 變 1，其他不變。

```
reg    = 0 0 0 0  0 0 0 0
BIT(3) = 0 0 0 0  1 0 0 0   ← 1 << 3
OR  ──────────────────────
result = 0 0 0 0  1 0 0 0   ← 只有 bit3 變 1，其他不動
```

---

### 清除 bit（Clear）：讓某個 bit 變 0

```c
uint32_t reg = 0b11111111;

// 清除 bit 3
reg = reg & ~(1 << 3);  // ~(0b00001000) = 0b11110111
reg &= ~(1 << 3);       // 簡寫
```

原理：mask 取反後只有 bit 3 是 0，AND 之後只有 bit 3 被清除。

```
reg     = 1 1 1 1  1 1 1 1
BIT(3)  = 0 0 0 0  1 0 0 0
~BIT(3) = 1 1 1 1  0 1 1 1   ← 只有 bit3 是 0
AND ──────────────────────
result  = 1 1 1 1  0 1 1 1   ← 只有 bit3 變 0，其他不動
```

---

### 切換 bit（Toggle）：0 變 1，1 變 0

```c
uint32_t reg = 0b10101010;

// 切換 bit 1
reg ^= (1 << 1);  // 0b10101000 → bit 1 從 1 變 0
reg ^= (1 << 1);  // 0b10101010 → 又變回 1
```

```
reg    = 1 0 1 0  1 0 1 0
BIT(1) = 0 0 0 0  0 0 1 0
XOR ──────────────────────
result = 1 0 1 0  1 0 0 0   ← bit1 翻轉（1→0）

再次 XOR：
result = 1 0 1 0  1 0 1 0   ← bit1 又翻回（0→1）
```

---

### 讀取 bit（Check）：某個 bit 是 0 還是 1

```c
uint32_t reg = 0b00001000;

// 讀取 bit 3
if (reg & (1 << 3)) {
    // bit 3 是 1
}

// 取出 bit 3 的值（0 或 1）
uint32_t bit3 = (reg >> 3) & 1;
```

---

## 常用 Macro 模式

嵌入式程式碼幾乎都會定義這些 macro：

```c
#define BIT(n)          (1UL << (n))
#define SET_BIT(reg, n)    ((reg) |=  BIT(n))
#define CLEAR_BIT(reg, n)  ((reg) &= ~BIT(n))
#define TOGGLE_BIT(reg, n) ((reg) ^=  BIT(n))
#define READ_BIT(reg, n)   (((reg) >> (n)) & 1UL)

// 使用
uint32_t status = 0;
SET_BIT(status, 4);         // 設定 bit 4
if (READ_BIT(status, 4)) {  // 讀取 bit 4
    CLEAR_BIT(status, 4);
}
```

---

## 多 bit 欄位（Bit Field）

暫存器常有多個 bit 組成一個欄位（field）：

**32-bit 暫存器欄位佈局示意：**

```
bit:  31          8  7      4  3    2  1    0
      ┌────────────┬──────────┬───────┬───────┐
      │  reserved  │   MODE   │ SPEED │ENABLE │
      │  (unused)  │ [7:4] 4b │[3:2]2b│[1:0]2b│
      └────────────┴──────────┴───────┴───────┘

MODE = 0b0101（值 5）設定在 bit4~7：
  0x5 << 4 = 0b0101_0000
```

```c
// 假設 CTRL 暫存器：
// bit [7:4] = MODE（4 bits）
// bit [3:2] = SPEED（2 bits）
// bit [1:0] = ENABLE（2 bits）

uint32_t ctrl = 0;

// 設定 MODE = 0b0101（在 bit 4–7）
#define MODE_SHIFT  4
#define MODE_MASK   (0xF << MODE_SHIFT)   // 0b11110000

ctrl &= ~MODE_MASK;            // 先清除舊值
ctrl |= (0x5 << MODE_SHIFT);   // 寫入新值

// 讀取 MODE
uint32_t mode = (ctrl & MODE_MASK) >> MODE_SHIFT;
```

---

## Struct Bitfield（C 的語法糖）

C 允許直接在 struct 裡定義 bit 寬度：

```c
typedef struct {
    uint32_t enable : 2;   // bit 0–1
    uint32_t speed  : 2;   // bit 2–3
    uint32_t mode   : 4;   // bit 4–7
    uint32_t        : 24;  // bit 8–31，保留
} CtrlReg;

CtrlReg ctrl = {0};
ctrl.mode = 0x5;
ctrl.enable = 1;
```

**注意**：bitfield 的 bit 排列順序是實作定義的（compiler-dependent），  
在嵌入式安全程式碼中，通常還是用手動 shift/mask，更可控。

---

## 實際案例：STM32 GPIO 設定

```c
// GPIO 模式暫存器（MODER），每個 pin 佔 2 bits
// Pin 5 的 mode 在 bit [11:10]
// 00 = Input, 01 = Output, 10 = Alternate, 11 = Analog

// GPIOA base address（RM0506 Table 9）
// 非安全別名：0x44240000，安全別名（M33 Secure）：0x54240000
volatile uint32_t *GPIOA_MODER = (volatile uint32_t *)0x54240000;

#define PIN5_SHIFT  10
#define PIN5_MASK   (0x3 << PIN5_SHIFT)

// 設定 PA5 為 Output（01）
*GPIOA_MODER &= ~PIN5_MASK;           // 清除
*GPIOA_MODER |= (0x1 << PIN5_SHIFT);  // 設定為 01
```

**STM32 GPIO MODER 暫存器佈局（每個 pin 佔 2 bits）：**

```
bit:  31 30 29 28  ... 11 10 9  8 7  6 5  4 3  2 1  0
      ┌────┬────┬──────┬────┬────┬────┬────┬────┬────┬────┬────┐
      │ P15│ P14│ ...  │ P5 │ P4 │ P3 │ P2 │ P1 │ P0 │ .. │    │
      └────┴────┴──────┴────┴────┴────┴────┴────┴────┴────┴────┘

每個 pin：  00=Input  01=Output  10=AF  11=Analog

PA5 的欄位在 bit[11:10]，設定為 Output（01）：
  shift = 5 * 2 = 10
  mask  = 0x3 << 10 = 0b0000_1100_0000_0000
  先清除舊值 &= ~mask
  寫入 01  |= (0x1 << 10)
```

這個模式在整個 STM32MP2 開發中會重複出現上千次。

---

## 常見錯誤

```c
// 錯誤 1：用 int 做 shift，可能 sign extension
int mask = 1 << 31;   // undefined behavior（有符號溢位）
uint32_t mask = 1U << 31;  // 正確，用 unsigned

// 錯誤 2：shift 超過型別寬度
uint8_t a = 1;
uint8_t result = a << 8;  // undefined behavior，shift 等於型別寬度

// 錯誤 3：忘記 mask 用 ~ 取反後型別
uint8_t reg = 0xFF;
reg &= ~(1 << 3);
// ~(1 << 3) 在 32-bit 系統是 0xFFFFFFF7，截斷成 uint8_t 是 0xF7，OK
// 但如果型別和 mask 不匹配要小心
```

---

## 下一步

> [!tip] 複習問題
> 1. 如何設定暫存器的 bit 3 而不影響其他 bit？如何清除它？各寫出一行程式碼。
> 2. 為什麼 shift 操作應該用 `1UL << n` 而不是 `1 << n`？對 `uint8_t` 做 `<< 8` 會發生什麼？
> 3. 讀取暫存器 bit [7:4] 欄位的值，寫出 mask 和 shift 計算。

→ [[04-function-pointers|Module 4：函式指標]]
