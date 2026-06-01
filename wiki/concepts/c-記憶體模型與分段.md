---
type: concept
title: C 記憶體模型與分段
tags: [C語言, 記憶體, linker, stack, heap, 嵌入式]
sources: [c-language-學習筆記]
created: 2026-05-25
updated: 2026-05-25
---

# C 記憶體模型與分段

## 一句話定義

嵌入式開發者必須清楚每個變數「住在哪裡」——記憶體用錯會造成 stack overflow、資料損毀、boot 失敗。

## 記憶體分段一覽

```
高位址  ┌─────────────────┐
        │      Stack      │ ← 函式呼叫、區域變數，往下長
        │        ↓        │
        │   （空閒區域）   │
        │        ↑        │
        │      Heap       │ ← malloc，往上長，嵌入式盡量不用
        ├─────────────────┤
        │      BSS        │ ← 未初始化全域/static 變數（全為 0）
        ├─────────────────┤
        │      Data       │ ← 已初始化全域/static 變數
        ├─────────────────┤
低位址  │      Text       │ ← 程式碼（唯讀，存在 Flash）
        └─────────────────┘
```

## 各段特性

| 段 | 存放 | 初始化 | 儲存位置 |
|----|------|--------|---------|
| Text | 程式碼 | — | Flash（唯讀）|
| Data | 已初始化全域/static | 由 Flash 複製到 RAM | Flash→RAM |
| BSS | 未初始化全域/static | startup code 清零 | RAM（不佔 Flash）|
| Stack | 區域變數、函式呼叫 | 無（垃圾值）| RAM |
| Heap | malloc | 無 | RAM |

**重要**：全域變數預設是 0，是因為 BSS 在開機時被 startup code 全清零。區域變數**不是** 0（stack 上的垃圾值）。

## 嵌入式記憶體分配策略

### 1. Static allocation（最常用）

```c
static uint8_t sha_buffer[256];  // 編譯時期決定大小和位址
```

優點：零碎片化，行為完全可預測。

### 2. Stack allocation（小型區域資料）

```c
void verify_block(void) {
    uint8_t hash[32];   // 小 buffer 才放 stack
}
```

**規則**：嵌入式 stack 通常只有 2–8KB，不要放大陣列。

### 3. 避免 malloc

原因：記憶體碎片化、不可預測的分配時機、安全關鍵程式碼要求確定性行為。需要動態分配時用 Memory Pool（預先分配固定數量 block）。

## Linker Script 概念

`.ld` 檔決定各段的實體位址：

- **VMA**（Virtual Memory Address）：程式執行時用的位址
- **LMA**（Load Memory Address）：資料存放位置（Flash）
- `.data` 段：LMA 在 Flash，VMA 在 RAM，開機時由 startup code 複製

## Startup Code 流程

`main()` 之前，CPU 執行（startup.s 或 crt0.c）：

```
1. 設定 Stack Pointer（指向 RAM 頂部）
2. 把 .data 從 Flash 複製到 RAM（LMA → VMA）
3. 把 .bss 清零
4. 呼叫 main()
```

RoT 專案的 M33 startup 是自己控制的，需要完全理解這個流程。

## 常見問題

```c
// Stack overflow：8KB 陣列在 stack 上 → 覆蓋其他資料
void bad() { uint8_t huge[8192]; }   // 改成 static
// 正確
void good() { static uint8_t huge[8192]; }  // 放 BSS

// 誤以為區域變數是 0
int x;           // stack 上的垃圾值，不是 0
int x = 0;       // 永遠明確初始化
```

## 相關頁面

- [[c-嵌入式關鍵語法]] — `__attribute__((section(...)))` 控制記憶體段
- [[c-多檔案組織與模組化]] — Linker script 與 Makefile
- [[c-未定義行為與安全程式碼]] — 未初始化變數的危險
