---
type: concept
title: C 多檔案組織與模組化
tags: [C語言, 模組化, Makefile, 嵌入式]
sources: [c-language-學習筆記]
created: 2026-05-25
updated: 2026-05-25
---

# C 多檔案組織與模組化

## 一句話定義

用 `.h`（宣告）+ `.c`（定義）拆分模組，以函式名稱前綴替代 namespace，讓嵌入式專案的幾十到幾百個 .c 檔保持可維護性。

## .h 和 .c 的職責

```
uart.h  → 對外說「我能做什麼」（型別、函式宣告）
uart.c  → 說明「我怎麼做」（實作細節）
```

- 外部模組只需 `#include "uart.h"`，不用知道 uart.c 的細節
- 等同 C# 的 `public` API + `private` 實作

## Include Guard（必須）

每個 .h 都要有，防止重複引入：

```c
#ifndef FLASH_H
#define FLASH_H

/* 內容 */

#endif /* FLASH_H */
```

## `static` 隱藏內部實作

```c
// flash.c
static uint32_t current_addr = FLASH_BASE;  // 外部看不到
static int is_valid_addr(uint32_t addr) {}  // 內部工具函式
```

等同 C# 的 `private`，強制透過公開函式存取。

## Opaque Pointer（不透明指標）

隱藏 struct 內部結構，等同 C# 的 private 欄位：

```c
// sha256.h：外部只知道型別存在
typedef struct Sha256Ctx Sha256Ctx;
void sha256_update(Sha256Ctx *ctx, const uint8_t *data, uint32_t len);

// sha256.c：完整定義只在這裡
struct Sha256Ctx { uint32_t state[8]; uint8_t buffer[64]; uint64_t count; };
```

## 命名慣例（替代 namespace）

```c
flash_init()    // 模組前綴小寫
FLASH_OK        // 常數全大寫
FlashResult     // 型別 PascalCase

// 不好
init()          // 哪個模組？
read()          // 和系統呼叫衝突
```

## Makefile 基礎

```makefile
CC     = arm-none-eabi-gcc
CFLAGS = -Wall -Wextra -Werror -O2 -mcpu=cortex-m33

firmware.elf: main.o flash.o uart.o
    $(CC) $(CFLAGS) -T linker.ld -o $@ $^

%.o: %.c
    $(CC) $(CFLAGS) -c -o $@ $<
```

## 典型 RoT 專案結構

```
rot-firmware/
├── src/
│   ├── startup.s      ← 向量表、初始化
│   ├── flash/flash.h + flash.c
│   ├── crypto/sha256.h + sha256.c
│   ├── boot/secure_boot.h + secure_boot.c  ← M33 驗證 A35 的邏輯
│   └── hal/gpio.h + gpio.c
└── include/common.h   ← 共用型別、macro
```

## 相關頁面

- [[c-基礎與型別系統]] — 宣告 vs 定義的基礎
- [[c-記憶體模型與分段]] — Linker script 與記憶體段
- [[c-函式指標與回調模式]] — 函式指標 struct 作為 interface
