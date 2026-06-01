---
tags: [c-language, module]
topic: c-language
week: "1-2"
---
# C 語言 Module 8：常見陷阱與 Undefined Behavior

C 的許多行為在規格書裡是「未定義」的（Undefined Behavior，UB）。  
UB 不一定會 crash，可能默默產生錯誤結果，在嵌入式安全程式碼裡非常危險。

---

## 整數相關陷阱

### 有符號整數溢位（UB）

```c
int32_t x = INT32_MAX;  // 2147483647
x = x + 1;             // Undefined Behavior！編譯器可以做任何事
                        // 實際上通常變成負數，但不保證

// 安全做法：用無符號（wraps around，行為確定）
uint32_t y = UINT32_MAX;
y = y + 1;  // 確定變成 0，這是定義好的行為
```

### 整數提升（Integer Promotion）

```c
uint8_t a = 200;
uint8_t b = 100;
uint8_t result = a + b;  // 先提升成 int 計算（300），再截斷成 uint8_t（44）

// 如果想要確定截斷行為，明確轉型
uint8_t result = (uint8_t)(a + b);
```

### 有符號右移

```c
int32_t x = -8;
x >> 1;   // 結果是 -4 還是 很大的正數？→ Implementation-defined
          // 大多數平台是算術右移（保留符號），但不保證

// 安全：只對 unsigned 做右移
uint32_t y = 0x80000000U;
y >> 1;   // 確定是 0x40000000
```

---

## 指標相關陷阱

### Strict Aliasing（別名規則）

```c
uint32_t reg_value = 0xDEADBEEF;

// 這樣做在許多情況是 UB（通過不同型別的指標存取同一記憶體）
float *fp = (float *)&reg_value;
float f = *fp;  // 可能被編譯器最佳化掉，因為它「認為」float 和 uint32 不會是同一記憶體

// 安全做法：用 memcpy（編譯器知道 memcpy 可以跨型別）
float f;
memcpy(&f, &reg_value, sizeof(float));

// 或用 union（C99 以後合法）
union { uint32_t u; float f; } conv;
conv.u = reg_value;
float result = conv.f;
```

嵌入式常需要把暫存器值解釋成不同型別，要用 union 或 memcpy。

### NULL Dereference

```c
uint8_t *buf = get_buffer();  // 可能回傳 NULL
*buf = 0xFF;  // 若 buf 是 NULL → crash（或寫到位址 0 的硬體！）

// 永遠檢查
if (buf == NULL) {
    handle_error();
    return;
}
*buf = 0xFF;
```

### 野指標（Wild Pointer）

```c
uint8_t *ptr;         // 未初始化，指向隨機位址
*ptr = 0;             // UB，可能覆蓋重要資料

uint8_t *ptr = NULL;  // 初始化為 NULL，至少 dereference 會 crash 而不是默默損壞
```

---

## 緩衝區溢位（Buffer Overflow）

在嵌入式和資安領域都是最危險的 bug：

```c
uint8_t buffer[16];

// 危險：沒有邊界檢查
void copy_data(const uint8_t *src, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        buffer[i] = src[i];  // 若 len > 16，覆蓋 buffer 後面的記憶體
    }
}

// 安全：加邊界檢查
void copy_data_safe(const uint8_t *src, uint32_t len) {
    uint32_t copy_len = len < sizeof(buffer) ? len : sizeof(buffer);
    for (uint32_t i = 0; i < copy_len; i++) {
        buffer[i] = src[i];
    }
}
```

RoT 的 firmware 驗證邏輯若有 buffer overflow，攻擊者可以控制執行流。

---

## Off-by-One 錯誤

```c
uint8_t arr[8];

// 錯誤：索引 0–8，但合法範圍是 0–7
for (int i = 0; i <= 8; i++) {
    arr[i] = 0;  // arr[8] 是 UB
}

// 正確
for (int i = 0; i < 8; i++) {
    arr[i] = 0;
}

// 更好：用 sizeof 避免 magic number
for (int i = 0; i < sizeof(arr); i++) {
    arr[i] = 0;
}
```

---

## 未初始化變數

```c
uint32_t status;        // stack 上的垃圾值

if (status == 0) {      // 行為未定義（讀取了未初始化的值）
    // ...
}

// 永遠初始化
uint32_t status = 0;
```

嵌入式程式碼慣例：宣告時立刻初始化。

---

## `memcpy` / `memset` 常見錯誤

```c
uint8_t dst[8];
uint8_t src[16];

// 錯誤：用 dst 的 size，但 sizeof(dst*) 是指標大小（4 或 8），不是陣列大小
memcpy(dst, src, sizeof(dst));    // OK，sizeof 陣列是正確的
memcpy(dst, src, sizeof(&dst));   // 錯！sizeof 指標 ≠ sizeof 陣列

// 函式參數中，陣列退化成指標，sizeof 失效
void bad_copy(uint8_t *dst, uint8_t *src) {
    memcpy(dst, src, sizeof(dst));  // sizeof(pointer) = 8，不是陣列大小
}
// 解法：明確傳入大小
void good_copy(uint8_t *dst, uint8_t *src, uint32_t len) {
    memcpy(dst, src, len);
}
```

---

## 整數截斷

```c
uint32_t large = 0x1FF;  // 511
uint8_t  small = large;  // 截斷為 0xFF（255），高位丟棄

// 若這是長度或 index，可能造成安全問題
uint32_t data_len = 0x100000100ULL;  // 攻擊者控制的值
uint8_t  len      = data_len;        // 截斷成 0，繞過長度檢查

// 防護：轉型前先範圍檢查
if (data_len > UINT8_MAX) {
    return ERROR_INVALID_LENGTH;
}
uint8_t len = (uint8_t)data_len;
```

---

## 比較有符號和無符號

```c
int32_t  signed_val   = -1;
uint32_t unsigned_val = 100;

if (signed_val < unsigned_val) {
    // 你以為這是 true，但實際上是 false！
    // -1 轉成 uint32_t 是 0xFFFFFFFF（4294967295），比 100 大
}
```

用 `-Wall` 編譯，GCC 會對這種比較發出警告。

---

## 使用 `-Wall -Wextra` 編譯

開啟所有警告，把警告當錯誤處理：

```makefile
CFLAGS = -Wall -Wextra -Werror -std=c99
```

常見有用的警告：
- `-Wuninitialized`：使用未初始化變數
- `-Wshadow`：區域變數遮蓋外層同名變數
- `-Wconversion`：隱式型別轉換
- `-Wformat`：printf 格式字串不匹配

---

## 小結：嵌入式安全程式碼的規則

1. 永遠初始化指標為 NULL，使用前檢查
2. 陣列存取前永遠檢查邊界
3. 跨型別存取記憶體用 `union` 或 `memcpy`
4. 只對 `unsigned` 做位元運算和右移
5. 整數截斷前先做範圍檢查
6. 開啟所有警告，warning as error
7. `sizeof` 只對陣列有效，傳進函式後要傳 `len` 參數

---

## 學習到此的總結

學完 Module 1–8，你具備：
- 看懂嵌入式 C 程式碼的能力
- 理解 AI 生成程式碼是否安全的判斷力
- 硬體暫存器操作的基礎
- 在面試中解釋自己程式碼的能力

下一步：→ `arm-architecture/` 目錄
