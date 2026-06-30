/*
 * 練習 04-01：函式指標基本用法
 *
 * 任務：
 *   實作一個簡單的計算機：
 *   1. 定義 typedef BinaryOp 為「接受兩個 int，回傳 int」的函式指標型別
 *   2. 實作 add, sub, mul 三個函式
 *   3. 用函式指標陣列 ops[] 存這三個函式
 *   4. 用迴圈印出 3 op 4 的結果
 */

#include <stdint.h>
#include <stdio.h>

/* TODO: typedef BinaryOp */
typedef int (*BinaryOp)(int, int);

/* TODO: 實作 add, sub, mul */
int add(int a, int b) { return a + b; }

int sub(int a, int b) { return a - b; }

int mul(int a, int b) { return a * b; }

int main(void) {
    /* TODO: 定義 ops[] 陣列，依序存 add, sub, mul */
    BinaryOp ops[3] = {add, sub, mul};
    const char *names[] = {"add", "sub", "mul"};

    for (int i = 0; i < 3; i++) {
        /* TODO: 用 ops[i] 呼叫，印出結果 */
        printf("3 %s 4 = %d\n", names[i], ops[i](3, 4));
        /* 格式：3 add 4 = 7 */
    }

    return 0;
}
