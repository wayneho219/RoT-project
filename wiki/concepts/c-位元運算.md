---
type: concept
title: C 位元運算
tags: [C語言, 位元運算, 暫存器, 嵌入式]
sources: [c-language-學習筆記]
created: 2026-05-25
updated: 2026-05-25
---

# C 位元運算

## 一句話定義

嵌入式最核心技能：硬體暫存器每個 bit 都有意義，需要能精準設定、清除、讀取任意 bit 而不影響其他 bit。

## 六個運算子

| 運算子 | 名稱 | 記憶 |
|--------|------|------|
| `&` | AND | 都是 1 才是 1（用於清除 bit、讀取 bit）|
| `\|` | OR | 有一個是 1 就是 1（用於設定 bit）|
| `^` | XOR | 不同才是 1（用於切換 bit）|
| `~` | NOT | 全部反轉（搭配 AND 清除 bit）|
| `<<` | 左移 | 乘以 2 的 n 次方（產生 mask）|
| `>>` | 右移 | 除以 2 的 n 次方（只對 unsigned 安全）|

## 四個核心操作

```c
// 設定 bit n（讓 bit 變 1）
reg |= (1 << n);

// 清除 bit n（讓 bit 變 0）
reg &= ~(1 << n);

// 切換 bit n（0→1，1→0）
reg ^= (1 << n);

// 讀取 bit n（回傳 0 或 1）
uint32_t val = (reg >> n) & 1;
```

## 標準 Macro 模式

嵌入式程式碼幾乎都定義：

```c
#define BIT(n)             (1UL << (n))
#define SET_BIT(reg, n)    ((reg) |=  BIT(n))
#define CLEAR_BIT(reg, n)  ((reg) &= ~BIT(n))
#define TOGGLE_BIT(reg, n) ((reg) ^=  BIT(n))
#define READ_BIT(reg, n)   (((reg) >> (n)) & 1UL)
```

## 多 bit 欄位（Bit Field）

暫存器常有多個 bit 組成一個欄位：

```c
#define MODE_SHIFT  4
#define MODE_MASK   (0xF << MODE_SHIFT)

reg &= ~MODE_MASK;           // 清除舊值
reg |= (新值 << MODE_SHIFT); // 寫入新值

uint32_t mode = (reg & MODE_MASK) >> MODE_SHIFT;  // 讀取
```

## STM32 GPIO 實例

GPIO MODER 暫存器：每個 pin 佔 2 bits（00=Input, 01=Output, 10=Alternate, 11=Analog）

```c
#define PIN5_SHIFT  10
#define PIN5_MASK   (0x3 << PIN5_SHIFT)

*GPIOA_MODER &= ~PIN5_MASK;          // 清除
*GPIOA_MODER |= (0x1 << PIN5_SHIFT); // 設定為 Output（01）
```

這個 read-modify-write 模式在 STM32 開發中會重複出現上千次。

## 常見錯誤

```c
// 錯誤：有符號整數左移到最高位 → UB
int mask = 1 << 31;
uint32_t mask = 1U << 31;  // 正確：用 unsigned

// 錯誤：shift 量等於或超過型別寬度 → UB
uint8_t a = 1;
uint8_t r = a << 8;  // shift 量 = 型別寬度 → UB
```

位元運算**永遠只對 unsigned 型別**操作（→ [[c-未定義行為與安全程式碼]]）。

## 相關頁面

- [[c-嵌入式關鍵語法]] — MMIO struct 模式（暫存器讀寫的另一種組織方式）
- [[c-未定義行為與安全程式碼]] — 有符號整數位元運算的 UB
