---
type: concept
title: C 指標與記憶體存取
tags: [C語言, 指標, 記憶體, 嵌入式]
sources: [c-language-學習筆記]
created: 2026-05-25
updated: 2026-05-25
---

# C 指標與記憶體存取

## 一句話定義

指標是「存記憶體位址的變數」；嵌入式的核心用途是直接讀寫硬體暫存器（特定記憶體位址）。

## 基本語法

```c
int value = 42;
int *ptr = &value;  // & 取位址
int n    = *ptr;    // * 取值（dereference）
*ptr     = 100;     // 透過指標修改 value
```

C# 對應：`ref` 參數，但 C 指標能做更多。

## 嵌入式核心用途：Memory-Mapped I/O

硬體暫存器 = 固定記憶體位址，用指標直接讀寫：

```c
volatile uint32_t *gpio_out = (volatile uint32_t *)0x50003000;
*gpio_out = 0x01;  // 寫暫存器（點亮 LED）
```

`volatile` 必要，防止編譯器最佳化掉硬體讀寫（→ [[c-嵌入式關鍵語法]]）。

## 指標與 Struct：`->` 運算子

```c
Device *ptr = &dev;
ptr->id = 1;       // 等同 (*ptr).id = 1，更常用
```

嵌入式大量使用 `->` 操作暫存器 struct。

## 指標算術

```c
uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
uint8_t *p = data;
*(p+1);  // 0x02，移動「一個元素的大小」，不是一個 byte
p[2];    // 0x03，陣列語法 = 指標語法
```

## 陣列傳入函式：Decay to Pointer

陣列傳入函式時自動 decay 成指標，**`sizeof` 在函式內失效**，必須明確傳 `len`：

```c
uint8_t data[5] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
find_byte(data, sizeof(data), 0xCC);  // sizeof(data) = 5，呼叫處有效

int find_byte(const uint8_t *data, uint32_t len, uint8_t target) {
    // sizeof(data) 在這裡 = 8（指標大小），不是 5
    for (uint32_t i = 0; i < len; i++) {
        if (*(data + i) == target) return (int)i;
    }
    return -1;
}
```

慣用簽名：`const uint8_t *data, uint32_t len`（唯讀輸入）；輸出 buffer 不加 `const`。

## `const` 指標的四種組合

| 宣告 | 指標可改 | 值可改 | 常見用途 |
|------|---------|--------|---------|
| `int *p` | ✓ | ✓ | 一般用途 |
| `const int *p` | ✓ | ✗ | 保護傳入資料（最常見）|
| `int * const p` | ✗ | ✓ | 固定位址的暫存器 |
| `const int * const p` | ✗ | ✗ | 最嚴格 |

函式參數 `const uint8_t *data`：「我拿到你的位址，但保證不修改」。

## 雙重指標

```c
int **pptr = &ptr;   // 指向指標的指標
**pptr;              // 兩次 dereference
```

嵌入式用途：函式修改指標本身（初始化 buffer 位址）。

## 常見錯誤

```c
int *p;              // 未初始化 → 指向隨機位址（野指標）
int *p = NULL;       // 正確：先設 NULL

int* get_value() {
    int local = 42;
    return &local;   // 函式結束後 local 消失 → 懸空指標
}
```

## 相關頁面

- [[c-嵌入式關鍵語法]] — volatile 詳解、Memory-Mapped I/O struct 模式
- [[c-記憶體模型與分段]] — 指標背後的記憶體佈局
- [[c-未定義行為與安全程式碼]] — NULL dereference、野指標等陷阱
