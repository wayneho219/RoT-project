---
tags: [c-language, module]
topic: c-language
week: "1-2"
---
# C 語言 Module 6：記憶體模型

嵌入式開發者必須清楚每個變數「住在哪裡」。  
記憶體用錯會造成 stack overflow、資料損毀、boot 失敗。

---

## 記憶體分段（Memory Sections）

一個 C 程式的記憶體分成幾個區域（由 linker script 決定位址）：

```
高位址  ┌─────────────────┐
        │      Stack      │  ← 往下長，函式呼叫、區域變數
        │        ↓        │
        │                 │
        │        ↑        │
        │      Heap       │  ← 往上長，malloc（嵌入式盡量不用）
        ├─────────────────┤
        │      BSS        │  ← 未初始化的全域/static 變數（全為 0）
        ├─────────────────┤
        │      Data       │  ← 已初始化的全域/static 變數
        ├─────────────────┤
低位址  │      Text       │  ← 程式碼（唯讀，存在 Flash）
        └─────────────────┘
```

---

## 各段說明

### Text（程式碼）

```c
void foo(void) { return; }  // 這個函式的機器碼在 Text 段
```

存在 Flash（NOR Flash），通常是唯讀的。

---

### Data（已初始化全域變數）

```c
uint32_t device_id = 0xABCD1234;  // 存在 Data 段
```

初始值存在 Flash，開機時由 startup code 複製到 RAM。

---

### BSS（未初始化全域變數）

```c
uint8_t rx_buffer[1024];  // 存在 BSS 段
static uint32_t counter;  // 也在 BSS
```

不佔 Flash 空間，開機時 startup code 把 BSS 全清為 0。  
**重要**：這就是為什麼全域變數在 C 裡預設是 0。

---

### Stack（堆疊）

```c
void process(void) {
    uint8_t local_buf[64];  // 在 Stack 上
    uint32_t temp = 0;      // 在 Stack 上
    // 函式返回後自動釋放
}
```

- 由 CPU 自動管理，進函式時往下長，返回時縮回
- 大小有限（嵌入式常常只有幾 KB）
- **Stack overflow**：用太多導致覆蓋其他資料，非常難 debug

---

### Heap（堆積）

```c
uint8_t *buf = malloc(256);  // 在 Heap 上
free(buf);                   // 手動釋放
```

嵌入式**盡量避免 malloc**，原因：
- 記憶體碎片化（長時間運行後可能 malloc 失敗）
- 不可預測的分配時機
- 安全關鍵程式碼要求確定性行為

---

## 嵌入式的記憶體分配策略

### 1. Static allocation（最常用）

```c
// 全域或 static，編譯時期就決定大小和位址
static uint8_t sha_buffer[256];
static uint8_t key_storage[32];
```

優點：零碎片化，大小和位址在編譯時期確定。

---

### 2. Stack allocation（小型區域資料）

```c
void verify_block(void) {
    uint8_t hash[32];        // 小 buffer 放 stack
    uint32_t result = 0;
    // 函式結束自動釋放
}
```

**規則**：不要在 stack 上放大陣列，嵌入式 stack 通常 2–8KB。

---

### 3. Memory Pool（進階，替代 malloc）

```c
// 預先分配固定數量的 block，從 pool 取用
#define POOL_SIZE 8
#define BLOCK_SIZE 64

static uint8_t pool_storage[POOL_SIZE][BLOCK_SIZE];
static uint8_t pool_used[POOL_SIZE];

uint8_t *pool_alloc(void) {
    for (int i = 0; i < POOL_SIZE; i++) {
        if (!pool_used[i]) {
            pool_used[i] = 1;
            return pool_storage[i];
        }
    }
    return NULL;  // pool 滿了
}

void pool_free(uint8_t *ptr) {
    for (int i = 0; i < POOL_SIZE; i++) {
        if (pool_storage[i] == ptr) {
            pool_used[i] = 0;
            return;
        }
    }
}
```

---

## Linker Script 概念

Linker script（`.ld` 檔）決定各段放在哪個實體位址：

```
/* 簡化範例 */
MEMORY {
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 2M   /* NOR Flash */
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 256K  /* SRAM */
}

SECTIONS {
    .text : { *(.text*) } > FLASH    /* 程式碼放 Flash */
    .data : { *(.data*) } > RAM AT > FLASH  /* 資料的 LMA 在 Flash，VMA 在 RAM */
    .bss  : { *(.bss*)  } > RAM      /* BSS 放 RAM */
}
```

- **VMA**（Virtual Memory Address）：程式執行時用的位址
- **LMA**（Load Memory Address）：資料存放的位址（Flash）
- 開機時 startup code 把 .data 從 LMA 複製到 VMA

這就是為什麼全域變數的初始值能從 Flash 搬到 RAM。

---

## Startup Code

開機後、`main()` 之前，CPU 執行 startup code（通常是 `startup.s` 或 `crt0.c`）：

```
1. 設定 Stack Pointer（指向 RAM 頂部）
2. 把 .data 從 Flash 複製到 RAM
3. 把 .bss 清零
4. 呼叫 main()
```

在 RoT 專案裡，你需要理解這個流程，因為 M33 的 startup 是你控制的。

---

## 常見問題

```c
// 問題 1：Stack overflow
void bad_function(void) {
    uint8_t huge_buffer[8192];  // 8KB 在 stack 上，可能超過 stack 大小
    // 解法：改成 static 或全域
    static uint8_t huge_buffer[8192];
}

// 問題 2：以為 local 變數初始化為 0
void function(void) {
    int x;           // stack 上的值是垃圾，不是 0
    printf("%d", x); // undefined behavior
    // 解法：明確初始化
    int x = 0;
}

// 問題 3：全域 vs static 全域 的差異
int global_a = 1;         // 所有 .c 檔都看得到
static int global_b = 1;  // 只有這個 .c 檔看得到（推薦）
```

---

## 下一步

→ [Module 7：多檔案組織](07-multi-file.md)
