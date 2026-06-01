/*
 * 練習 01：固定寬度型別與記憶體大小
 *
 * 目標：
 *   了解 uint8_t / uint16_t / uint32_t 的實際大小，
 *   以及為什麼嵌入式不用裸的 int。
 *
 * 任務：
 *   填入 TODO 的部分，讓輸出符合 ex01.expected 的內容。
 */

#include <stdint.h>
#include <stdio.h>

int main(void) {
    /* TODO: 宣告以下變數，型別使用固定寬度型別
     *   - 名稱 a，能存 255（最大值），值設為 255
     *   - 名稱 b，能存 65535（最大值），值設為 65535
     *   - 名稱 c，值設為 0xDEADBEEF
     *   - 名稱 d，有符號，值設為 -1
     */

    /* TODO: 填入正確型別 */
    uint8_t a = 255;
    uint16_t b = 65535;
    uint32_t c = 0xDEADBEEF;
    int32_t d = -1;

    printf("a = %u, size = %zu bytes\n", a, sizeof(a));
    printf("b = %u, size = %zu bytes\n", b, sizeof(b));
    printf("c = 0x%X, size = %zu bytes\n", c, sizeof(c));
    printf("d = %d, size = %zu bytes\n", d, sizeof(d));

    return 0;
}
