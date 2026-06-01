---
tags: [c-language, module]
topic: c-language
week: "1-2"
---
# C 語言 Module 2：指標與記憶體

## 核心概念

記憶體是一個大陣列，每個格子有位址（address）。  
指標就是「存位址的變數」。

```
記憶體：
位址  0x1000  0x1001  0x1002  0x1003
值    [  42  ][  0   ][  0   ][  0  ]

int value = 42;   → 存在位址 0x1000
int *ptr = &value → ptr 的值是 0x1000
```

---

## 基本語法

```c
int value = 42;

int *ptr = &value;  // & 取位址：ptr = 0x1000（value 的位址）
int n = *ptr;       // * 取值（dereference）：n = 42
*ptr = 100;         // 透過指標修改 value → value 現在是 100
```

宣告語法：`型別 *變數名`（`*` 靠近變數名，是慣例）

```c
int *p;      // 指向 int 的指標
uint8_t *q;  // 指向 uint8_t 的指標
```

---

## 指標與函式參數

C 的參數傳遞是**傳值（pass by value）**，函式拿到的是副本。  
要讓函式修改外部變數，傳指標進去：

```c
// 這樣沒用：n 是副本
void double_value_wrong(int n) {
    n = n * 2;  // 只改了副本
}

// 這樣才對：傳指標
void double_value(int *n) {
    *n = *n * 2;  // 透過指標改到原本的變數
}

int main(void) {
    int x = 5;
    double_value(&x);   // 傳 x 的位址
    printf("%d\n", x);  // 10
}
```

C# 對應：`ref` 參數。

---

## 指標與 Struct

兩種存取方式：

```c
typedef struct {
    uint32_t id;
    uint8_t  status;
} Device;

Device dev;
Device *ptr = &dev;

// 方法 1：先 dereference 再存取
(*ptr).id = 1;

// 方法 2：-> 運算子（等價，更常用）
ptr->id = 1;
ptr->status = 0xFF;
```

嵌入式大量使用 `->` 來操作硬體暫存器 struct。

---

## 指標與陣列

陣列名稱本身就是第一個元素的位址：

```c
uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};

uint8_t *p = data;      // 等同 &data[0]
printf("%d\n", *p);     // 0x01
printf("%d\n", *(p+1)); // 0x02，指標算術
printf("%d\n", p[2]);   // 0x03，陣列語法和指標語法等價
```

**指標算術**：`p + 1` 移動的是「一個元素的大小」，不是一個 byte。

```c
uint32_t regs[4];
uint32_t *p = regs;
// p + 1 往後移 4 bytes（sizeof(uint32_t) = 4）
```

---

## 陣列傳入函式：Decay to Pointer

陣列名稱在傳入函式時會**自動變成指向第一個元素的指標**（decay），這是 C 最常見的混淆點：

```c
uint8_t data[5] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};

// 呼叫時：data 等同 &data[0]，自動 decay 成 uint8_t *
find_byte(data, sizeof(data), 0xCC);

// 函式簽名：收到的是指標，不是陣列
int find_byte(const uint8_t *data, uint32_t len, uint8_t target) {
    // sizeof(data) 在這裡是指標大小（4 或 8 bytes），不是 5！
    // 所以必須明確傳 len
}
```

**結論**：
- 傳陣列進函式時，**一定要同時傳長度**（`len` 參數）
- 函式裡絕對不能用 `sizeof(陣列指標)` 取長度

### `const uint8_t *data, uint32_t len` 慣用模式

嵌入式幾乎所有「處理一段資料」的函式都用這個簽名：

```c
// 唯讀資料用 const（保護呼叫者的資料）
int verify_block(const uint8_t *data, uint32_t len);

// 可寫入的輸出 buffer 不加 const
void hash_compute(const uint8_t *input, uint32_t in_len,
                  uint8_t *output, uint32_t out_len);
```

---

## 嵌入式核心用途：Memory-Mapped I/O

硬體暫存器就是固定記憶體位址，用指標直接讀寫：

```c
// 把位址 0x50003000 當成 uint32_t 的指標
volatile uint32_t *gpio_out = (volatile uint32_t *)0x50003000;

*gpio_out = 0x01;        // 寫暫存器
uint32_t val = *gpio_out; // 讀暫存器
```

`volatile` 的用途：告訴編譯器「這個值可能被硬體改變，不要最佳化掉」。  
（詳見 Module 5）

---

## 雙重指標

指向指標的指標：

```c
int value = 42;
int *ptr = &value;
int **pptr = &ptr;  // pptr 存的是 ptr 的位址

printf("%d\n", **pptr);  // 42，兩次 dereference
```

嵌入式常見用途：函式修改指標本身（例如初始化 buffer 位址）：

```c
void init_buffer(uint8_t **buf, uint32_t size) {
    // 讓呼叫者的指標指向靜態 buffer
    static uint8_t storage[256];
    *buf = storage;
}

uint8_t *my_buf = NULL;
init_buffer(&my_buf, 256);
// 現在 my_buf 指向 storage
```

---

## NULL 指標

```c
int *ptr = NULL;  // 代表「沒有指向任何東西」

// 使用前要檢查
if (ptr != NULL) {
    *ptr = 42;
}
```

對 NULL dereference 會造成 crash（或更糟：在嵌入式上寫到位址 0 的硬體）。

---

## `const` 與指標（四種組合）

```c
int x = 10;
int y = 20;

int *p = &x;                  // 指標可改，值可改
const int *p = &x;            // 指標可改，值不可改（保護資料）
int * const p = &x;           // 指標不可改，值可改
const int * const p = &x;     // 指標不可改，值不可改（最嚴格）
```

函式參數最常見的是 `const uint8_t *data`：  
「我拿到你的資料位址，但我保證不修改它」。

---

## 常見錯誤

```c
// 錯誤 1：未初始化的指標（dangling/wild pointer）
int *p;        // p 是垃圾值
*p = 42;       // 寫到隨機位址 → undefined behavior

// 正確：先初始化
int *p = NULL;

// 錯誤 2：回傳 local 變數的位址
int* get_value(void) {
    int local = 42;
    return &local;  // local 在函式結束後消失，指標變懸空
}

// 錯誤 3：指標型別不匹配
uint32_t reg = 0xDEADBEEF;
uint8_t *p = (uint8_t *)&reg;  // 合法但要小心 endianness
```

---

## 下一步

→ [Module 3：Bit Manipulation](03-bit-manipulation.md)
