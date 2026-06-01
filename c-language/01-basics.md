---
tags: [c-language, module]
topic: c-language
week: "1-2"
---
# C 語言 Module 1：基礎概念（從 C# 切入）

## C 沒有的東西

| C# | C |
|----|---|
| Class、物件、繼承 | 只有 `struct`（純資料，沒有方法） |
| Garbage Collector | 你自己管記憶體 |
| `string` 型別 | 只有 `char` 陣列 + `\0` 結尾 |
| Exception | 只有回傳值和 `errno` |
| `namespace` | 用 `static` 限制可見範圍 |
| `bool`（內建） | C99 後有 `<stdbool.h>`，否則用 `int` |

嵌入式 C 連標準函式庫（`printf`、`malloc`）也常不能用。

---

## 型別系統

### 基本型別

```c
char    c = 'A';       // 1 byte
int     n = 42;        // 通常 4 bytes，但平台決定
short   s = 100;       // 通常 2 bytes
long    l = 100000L;   // 通常 4 或 8 bytes
float   f = 3.14f;
double  d = 3.14;
```

### 固定寬度型別（嵌入式必用）

`int` 的大小隨平台變動，嵌入式一律改用：

```c
#include <stdint.h>

uint8_t   a = 255;         // 無符號 8-bit  → C# 的 byte
uint16_t  b = 65535;       // 無符號 16-bit → C# 的 ushort
uint32_t  c = 0xDEADBEEF;  // 無符號 32-bit，暫存器操作最常用
int32_t   d = -1;          // 有符號 32-bit → C# 的 int
```

> **規則**：嵌入式程式碼只用 `uintN_t` / `intN_t`，不用裸的 `int`

### 為什麼暫存器操作用 `uint32_t`

- 暫存器沒有「負數」概念
- Bit shift 在 unsigned 行為明確，signed 會有未定義行為

---

## 函式

```c
// C# 方法
public int Add(int a, int b) { return a + b; }

// C 函式（全域，沒有 class）
int add(int a, int b) { return a + b; }
```

### 宣告 vs 定義

C 需要先宣告才能呼叫（C# 沒有這個限制）：

```c
// 宣告（放在 .h 檔或檔案最上方）
int add(int a, int b);

// 定義（實作，放在 .c 檔）
int add(int a, int b) {
    return a + b;
}
```

---

## Struct

只有資料，沒有方法：

```c
// 基本寫法
struct Point {
    int x;
    int y;
};
struct Point p;
p.x = 10;

// 實務寫法（用 typedef 省掉每次寫 struct）
typedef struct {
    uint32_t x;
    uint32_t y;
} Point;

Point p;
p.x = 10;
```

嵌入式用 struct 對應硬體暫存器（詳見 `stm32mp2/` 筆記）。

---

## Enum（列舉型別）

用來定義一組具名的整數常數，最適合狀態機和錯誤碼：

```c
typedef enum {
    BOOT_IDLE       = 0,
    BOOT_READING,       // 自動 = 1
    BOOT_VERIFYING,     // 自動 = 2
    BOOT_BOOTING,
    BOOT_DONE,
    BOOT_ERROR,
} BootState;

BootState state = BOOT_IDLE;
if (state == BOOT_ERROR) { /* ... */ }
```

C# 對應：`enum`，但 C 的 enum 底層是 `int`，可直接當陣列索引。

**字串對應表（常用技巧）**：把 enum 值對應到可讀名稱：

```c
static const char *state_names[] = {
    "IDLE", "READING", "VERIFYING", "BOOTING", "DONE", "ERROR"
};
printf("state: %s\n", state_names[state]);  // 不用 switch/if-else
```

---

## C 字串（char 陣列）

C 沒有 `string` 型別，字串是以 `\0`（null terminator）結尾的 `char` 陣列：

```c
char name[16] = "hello";    // 自動補 \0，實際佔 6 bytes
char *p = "hello";          // 字串字面值（唯讀）
```

### 字串常用函式（`#include <string.h>`）

```c
strlen(s)                   // 長度（不含 \0）
strcmp(a, b)                // 相等回傳 0，< 0 表示 a < b，> 0 表示 a > b
strncpy(dst, src, n)        // 複製最多 n 個字元（不保證 \0 結尾！）
```

**`strncpy` 的正確用法**：

```c
#define KEY_MAX_LEN 15

char key[KEY_MAX_LEN + 1];                 // +1 留給 \0
strncpy(key, src, KEY_MAX_LEN);            // 最多複製 15 個字元
key[KEY_MAX_LEN] = '\0';                   // 強制補 \0，確保安全
```

### Struct 裡的 char 陣列

```c
typedef struct {
    char     key[16];       // 固定大小，不是指標
    uint32_t value;
} ConfigEntry;

ConfigEntry e;
strncpy(e.key, "fw_version", 15);
e.key[15] = '\0';
e.value = 3;

// 比較
if (strcmp(e.key, "fw_version") == 0) { /* 找到了 */ }
```

**char 陣列 vs char 指標**：

| | `char name[16]` | `const char *name` |
|--|------------------|--------------------|
| 記憶體 | 在 struct/stack 內 | 指向別處 |
| 可寫入 | 是 | 否（字面值唯讀）|
| sizeof | 16 | 4 或 8（指標大小）|
| 嵌入式慣例 | 儲存可變字串 | 儲存唯讀名稱（如 driver name）|

---

## 手動位元組序列化（Byte Packing）

嵌入式常需要把資料結構轉換成位元組陣列（例如寫入 Flash、透過通訊協定傳送），不能直接用 `memcpy` 複製整個 struct（struct 可能有 padding）。

### Little-Endian：uint32_t ↔ 4 bytes

大多數嵌入式平台（ARM Cortex-M）是 little-endian：低位 byte 存在低位址。

```c
uint32_t value = 0x00018000;

// 拆解成 4 bytes（序列化）
buf[0] = (uint8_t)(value & 0xFF);           // 0x00（最低位）
buf[1] = (uint8_t)((value >>  8) & 0xFF);   // 0x80
buf[2] = (uint8_t)((value >> 16) & 0xFF);   // 0x01
buf[3] = (uint8_t)((value >> 24) & 0xFF);   // 0x00（最高位）

// 組合回 uint32_t（反序列化）
uint32_t restored =   (uint32_t)buf[0]
                    | ((uint32_t)buf[1] <<  8)
                    | ((uint32_t)buf[2] << 16)
                    | ((uint32_t)buf[3] << 24);
```

**記憶方式**：序列化時從低位往高位 shift；反序列化時從低索引往高索引 shift。

### 完整範例：序列化 firmware header

```c
typedef struct {
    uint8_t  magic;
    uint8_t  version;
    uint32_t image_size;
} Header;

void header_serialize(const Header *h, uint8_t out[6]) {
    out[0] = h->magic;
    out[1] = h->version;
    out[2] = (uint8_t)(h->image_size & 0xFF);
    out[3] = (uint8_t)((h->image_size >>  8) & 0xFF);
    out[4] = (uint8_t)((h->image_size >> 16) & 0xFF);
    out[5] = (uint8_t)((h->image_size >> 24) & 0xFF);
}

Header header_deserialize(const uint8_t data[6]) {
    Header h;
    h.magic   = data[0];
    h.version = data[1];
    h.image_size =   (uint32_t)data[2]
                   | ((uint32_t)data[3] <<  8)
                   | ((uint32_t)data[4] << 16)
                   | ((uint32_t)data[5] << 24);
    return h;
}
```

---

## 指標初探

**指標 = 存記憶體位址的變數**（細節在 Module 2）

```c
int value = 42;
int *ptr = &value;   // & 取位址，ptr 存的是 value 的位址

printf("%d\n", *ptr);  // * 取值 → 42
*ptr = 100;            // 透過指標修改 value
printf("%d\n", value); // 100
```

C# 對應：`ref` 參數，但 C 的指標能做更多。

**嵌入式核心用途**：硬體暫存器就是特定記憶體位址

```c
// GPIO 輸出暫存器在位址 0x50003000
uint32_t *gpio_out = (uint32_t *)0x50003000;
*gpio_out = 0x01;  // 設定 bit 0 = 1，讓 LED 亮
```

---

## `const` 和 `static`

### `const`

```c
const uint32_t MAX = 100;  // 不可修改

// 函式參數：保證不修改傳入的資料
void process(const uint8_t *data, uint32_t len);
```

### `static`（兩種語意）

```c
// 1. 限制可見範圍（只在這個 .c 檔看得到）→ 類似 C# private
static int internal_counter = 0;

// 2. 函式內：保留上次的值，不因函式返回而消失
void count() {
    static int n = 0;  // 只初始化一次
    n++;               // 每次呼叫都累加
}
```

---

## 標頭檔（.h）

```
module.h  → 宣告（對外公開什麼）
module.c  → 定義（實作細節）
```

防重複引入的標準寫法（Include Guard）：

```c
// module.h
#ifndef MODULE_H
#define MODULE_H

void module_init(void);
uint32_t module_read(void);

#endif
```

---

## 快速對照表

| C# | C |
|----|---|
| `class` | `struct` + 相關函式 |
| `private` | `static` 在 .c 檔內 |
| `interface` | 函式指標 struct（Module 4） |
| `enum` | `typedef enum { ... } Name;` |
| `string` | `char` 陣列 + `\0` 結尾 |
| `new` / `delete` | `malloc` / `free`（嵌入式幾乎不用）|
| `using` | `#include` |
| `namespace` | 函式名稱加前綴（如 `gpio_init`）|
| `try/catch` | 回傳錯誤碼（`if (ret != 0) ...`）|

---

## 下一步

→ [Module 2：指標與記憶體](02-pointers.md)
