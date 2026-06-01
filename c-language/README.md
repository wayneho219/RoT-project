---
tags: [c-language, index]
topic: c-language
week: "1-2"
---
# C 語言學習筆記

## 閱讀順序

| 模組 | 主題 | 練習 |
|------|------|------|
| [01-basics](01-basics.md) | 基礎概念、型別、struct | practice/module-01/ |
| [02-pointers](02-pointers.md) | 指標、記憶體存取、struct 指標 | practice/module-02/ |
| [03-bit-manipulation](03-bit-manipulation.md) | 位元運算、暫存器操作 | practice/module-03/ |
| [04-function-pointers](04-function-pointers.md) | 函式指標、callback、jump table | practice/module-04/ |
| [05-embedded-patterns](05-embedded-patterns.md) | volatile、macro、memory-mapped I/O | — |
| [06-memory-model](06-memory-model.md) | stack/heap/BSS、linker、startup | — |
| [07-multi-file](07-multi-file.md) | .h/.c 分割、模組化、Makefile | — |
| [08-pitfalls](08-pitfalls.md) | UB、buffer overflow、整數陷阱 | — |

## 練習用法

```bash
cd practice

# 編譯並執行
make run FILE=module-01/ex01

# 驗證輸出
make check FILE=module-01/ex01

# 清除編譯產物
make clean
```

## 學習重點對照

| 嵌入式需求 | 對應模組 |
|-----------|---------|
| 暫存器讀寫 | 02, 03, 05 |
| 硬體驅動設計 | 04, 07 |
| boot 流程理解 | 06 |
| 安全程式碼 | 08 |
