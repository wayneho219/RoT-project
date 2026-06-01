---
type: source
title: C 語言學習筆記（嵌入式導向，從 C# 切入）
tags: [C語言, 嵌入式, 學習筆記]
authors: [自製筆記]
raw: c-language/
ingested: 2026-05-25
created: 2026-05-25
updated: 2026-05-25
---

# C 語言學習筆記系列

## 概述

8 個 module 的嵌入式 C 學習筆記，以 C# 開發者視角切入，目標是能讀懂並撰寫嵌入式（STM32/RoT）的 C 程式碼。附有 `practice/` 練習題（module 01–04）。

## 閱讀順序與對應概念頁

| Module | 主題 | Wiki 概念頁 |
|--------|------|------------|
| 01 | 基礎、型別、struct | [[c-基礎與型別系統]] |
| 02 | 指標、記憶體存取 | [[c-指標與記憶體存取]] |
| 03 | 位元運算、暫存器操作 | [[c-位元運算]] |
| 04 | 函式指標、callback、jump table | [[c-函式指標與回調模式]] |
| 05 | volatile、macro、Memory-Mapped I/O | [[c-嵌入式關鍵語法]] |
| 06 | stack/heap/BSS、linker、startup | [[c-記憶體模型與分段]] |
| 07 | .h/.c 分割、模組化、Makefile | [[c-多檔案組織與模組化]] |
| 08 | UB、buffer overflow、整數陷阱 | [[c-未定義行為與安全程式碼]] |

## 嵌入式學習重點對照

| 嵌入式需求 | 對應模組 |
|-----------|---------|
| 暫存器讀寫 | 02, 03, 05 |
| 硬體驅動設計 | 04, 07 |
| boot 流程理解 | 06 |
| 安全程式碼 | 08 |

## 關鍵洞見（從 C# 轉換視角）

- C 沒有 GC、沒有 class、沒有 exception，一切靠明確的 struct + 函式 + 回傳值
- 嵌入式 C 連標準函式庫（`printf`、`malloc`）都常不能用
- `uint32_t` 等固定寬度型別是嵌入式必用（`int` 大小平台相依）
- 函式指標 struct = C 的 interface（C# 的 `interface`）
- 記憶體每個 byte 都要親自管，stack overflow 是實際危險

## 練習題位置

`c-language/practice/`，module 01–04 有練習：
```bash
cd c-language/practice
make run FILE=module-01/ex01
make check FILE=module-01/ex01
```
