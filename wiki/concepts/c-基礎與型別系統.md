---
type: concept
title: C 基礎與型別系統
tags: [C語言, 型別, struct, 嵌入式]
sources: [c-language-學習筆記]
created: 2026-05-25
updated: 2026-05-25
---

# C 基礎與型別系統

## 一句話定義

C 是沒有 GC、沒有 class、沒有 exception 的低階語言；嵌入式 C 更連標準函式庫都不能用，一切靠 struct + 函式 + 明確的記憶體管理。

## C# vs C 對照

| C# | C |
|----|---|
| `class` | `struct`（純資料，沒有方法）|
| Garbage Collector | 自己管記憶體 |
| `string` | `char` 陣列 + `\0` 結尾 |
| `enum` | `typedef enum { ... } Name;` |
| Exception | 回傳值 + `errno` |
| `namespace` | 函式名稱前綴（如 `gpio_init`）|
| `private` | `static` 限制 .c 檔內可見 |
| `interface` | 函式指標 struct（→ [[c-函式指標與回調模式]]）|
| `new` / `delete` | `malloc` / `free`（嵌入式幾乎不用）|
| `bool` | C99 後 `<stdbool.h>`，否則用 `int` |

## 固定寬度型別（嵌入式必用）

`int` 大小隨平台變動，嵌入式**一律**改用：

```c
#include <stdint.h>
uint8_t   // 1 byte，C# 的 byte
uint16_t  // 2 bytes，C# 的 ushort
uint32_t  // 4 bytes，暫存器操作最常用
int32_t   // 有符號 32-bit，C# 的 int
```

**為什麼暫存器用 `uint32_t`**：暫存器沒有負數概念，unsigned 的 bit shift 行為明確，signed 會有 UB（→ [[c-未定義行為與安全程式碼]]）。

## Struct

只有資料，沒有方法。實務用 `typedef` 省略每次寫 `struct`：

```c
typedef struct {
    uint32_t x;
    uint32_t y;
} Point;
```

嵌入式用 struct 對應硬體暫存器群組（→ [[c-嵌入式關鍵語法]]）。

## `static` 的兩種語意

```c
static int n = 0;        // 1. 限制可見範圍到此 .c 檔
void count() {
    static int n = 0;    // 2. 函式內：跨呼叫保留值（只初始化一次）
}
```

## 宣告 vs 定義

C 需要先宣告才能呼叫（C# 沒有此限制）：
- 宣告（declaration）放 `.h` 檔
- 定義（definition）放 `.c` 檔

## Enum

```c
typedef enum {
    BOOT_IDLE = 0, BOOT_READING, BOOT_VERIFYING, BOOT_DONE, BOOT_ERROR,
} BootState;

// 字串對應表（不用 switch/if-else）
static const char *state_names[] = { "IDLE", "READING", "VERIFYING", "DONE", "ERROR" };
printf("%s\n", state_names[state]);
```

## C 字串

固定長度 char 陣列 in struct（嵌入式慣例，不用 `char *`）：

```c
typedef struct { char key[16]; uint32_t value; } ConfigEntry;

strncpy(e.key, "fw_version", 15);
e.key[15] = '\0';                       // 永遠強制補 \0
if (strcmp(e.key, "fw_version") == 0)   // 比較
```

## 手動位元組序列化（Little-Endian）

struct 有 padding，不能直接 `memcpy` 整個 struct 到 byte 陣列：

```c
// uint32_t → 4 bytes（序列化）
buf[0] = (uint8_t)(value & 0xFF);
buf[1] = (uint8_t)((value >>  8) & 0xFF);
buf[2] = (uint8_t)((value >> 16) & 0xFF);
buf[3] = (uint8_t)((value >> 24) & 0xFF);

// 4 bytes → uint32_t（反序列化）
uint32_t v = (uint32_t)buf[0] | ((uint32_t)buf[1]<<8)
           | ((uint32_t)buf[2]<<16) | ((uint32_t)buf[3]<<24);
```

## 相關頁面

- [[c-指標與記憶體存取]] — 指標基礎、陣列 decay
- [[c-函式指標與回調模式]] — 模擬 interface
- [[c-多檔案組織與模組化]] — .h/.c 分工詳解
- [[c-位元運算]] — Byte packing 用到的 shift 技巧
