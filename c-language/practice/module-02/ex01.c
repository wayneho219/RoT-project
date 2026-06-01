/*
 * 練習 02-01：指標基本操作
 *
 * 任務：
 *   1. 用指標交換兩個變數的值（實作 swap）
 *   2. 用指標讓函式回傳兩個值（同時計算 sum 和 product）
 */

#include <stdint.h>
#include <stdio.h>

/* TODO: 實作 swap，交換 *a 和 *b 的值 */
void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

/* TODO: 實作 sum_and_product
 *   計算 a + b，結果存入 *sum
 *   計算 a * b，結果存入 *product
 */
void sum_and_product(int a, int b, int *sum, int *product) {
    *sum = a + b;
    *product = a * b;
}

int main(void) {
    int x = 10, y = 20;
    swap(&x, &y);
    printf("x=%d y=%d\n", x, y);

    int s, p;
    sum_and_product(3, 4, &s, &p);
    printf("sum=%d product=%d\n", s, p);

    return 0;
}
