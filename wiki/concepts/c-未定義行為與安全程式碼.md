---
type: concept
title: C 未定義行為與安全程式碼
tags: [C語言, UB, 安全, 嵌入式, buffer-overflow]
sources: [c-language-學習筆記]
created: 2026-05-25
updated: 2026-05-25
---

# C 未定義行為與安全程式碼

## 一句話定義

C 的 Undefined Behavior（UB）不一定 crash，可能默默產生錯誤結果；嵌入式安全程式碼（如 RoT）中，UB 可能讓攻擊者控制執行流。

## 整數相關 UB

### 有符號整數溢位

```c
int32_t x = INT32_MAX;
x = x + 1;   // UB！編譯器可以做任何事

uint32_t y = UINT32_MAX;
y = y + 1;   // 確定變成 0（unsigned wraps around）
```

**規則**：嵌入式一律用 `uintN_t` 做算術，有符號整數不做溢位運算。

### 整數提升（Integer Promotion）

```c
uint8_t a = 200, b = 100;
uint8_t result = a + b;  // 先提升成 int 計算（300），再截斷成 uint8_t（44）
uint8_t result = (uint8_t)(a + b);  // 明確轉型，意圖清楚
```

### 有符號右移

```c
int32_t x = -8;
x >> 1;   // Implementation-defined（大多數平台是算術右移，但不保證）
uint32_t y = 0x80000000U;
y >> 1;   // 確定是 0x40000000（unsigned 邏輯右移）
```

## 指標相關 UB

### Strict Aliasing（通過不同型別指標存取同一記憶體）

```c
uint32_t reg = 0xDEADBEEF;
float *fp = (float *)&reg;  // UB

// 安全做法 1：memcpy
float f; memcpy(&f, &reg, sizeof(float));

// 安全做法 2：union（C99 合法）
union { uint32_t u; float f; } conv;
conv.u = reg; float result = conv.f;
```

嵌入式需要把暫存器值解釋成不同型別時，用 union 或 memcpy。

## 緩衝區溢位（最危險）

```c
// 危險：沒有邊界檢查
buffer[i] = src[i];   // len > sizeof(buffer) → 覆蓋後面的記憶體

// 安全
uint32_t copy_len = len < sizeof(buffer) ? len : sizeof(buffer);
```

RoT firmware 驗證邏輯若有 buffer overflow，攻擊者可以控制執行流。

## 整數截斷（安全漏洞）

```c
uint32_t data_len = 0x100000100ULL;  // 攻擊者控制的值
uint8_t  len      = data_len;         // 截斷成 0 → 繞過長度檢查！

// 防護：轉型前先範圍檢查
if (data_len > UINT8_MAX) return ERROR_INVALID_LENGTH;
uint8_t len = (uint8_t)data_len;
```

## 有符號 vs 無符號比較（隱藏 bug）

```c
int32_t  signed_val   = -1;
uint32_t unsigned_val = 100;
if (signed_val < unsigned_val)  // 你以為是 true，但實際是 false！
// -1 轉成 uint32_t = 0xFFFFFFFF = 4294967295 > 100
```

用 `-Wall` 編譯，GCC 會對此發出警告。

## 嵌入式安全程式碼的 7 條規則

1. 永遠初始化指標為 NULL，使用前檢查
2. 陣列存取前永遠檢查邊界
3. 跨型別存取記憶體用 `union` 或 `memcpy`
4. **只對 unsigned 做位元運算和右移**
5. 整數截斷前先做範圍檢查
6. 開啟所有警告：`-Wall -Wextra -Werror`
7. `sizeof` 只對陣列名稱有效；傳進函式後要傳 `len` 參數

## 相關頁面

- [[c-位元運算]] — 位元運算只對 unsigned
- [[c-指標與記憶體存取]] — NULL dereference、野指標
- [[c-記憶體模型與分段]] — 未初始化區域變數的垃圾值
