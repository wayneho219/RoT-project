/*
 * 練習 03：static 的兩種用法
 *
 * 任務：
 *   1. 實作 increment()：每次呼叫讓內部計數器 +1，並回傳當前值
 *      （用 static 區域變數，不可用全域變數）
 *
 *   2. 在這個檔案底部有一個 internal_value = 42
 *      在 main() 裡直接用 printf 印出它
 *      思考：如果這是另一個 .c 檔，你能存取它嗎？
 */

#include <stdint.h>
#include <stdio.h>

/* TODO: 實作 increment()，每次呼叫回傳遞增的值（從 1 開始）*/
int increment(void) {
    static int now = 0;
    now++;
    return now;
}

/* 這個變數只有這個 .c 檔能看到（加了 static）*/
static int internal_value = 42;

int main(void) {
    printf("call 1: %d\n", increment());
    printf("call 2: %d\n", increment());
    printf("call 3: %d\n", increment());
    printf("internal_value: %d\n", internal_value);

    return 0;
}
