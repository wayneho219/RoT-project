/*
 * 練習 02-03：指標與陣列
 *
 * 情境：
 *   模擬從 flash 讀取一段 firmware header，
 *   用指標算術逐一解析每個 byte。
 *
 * 任務：
 *   給定 uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE}
 *
 *   1. 用指標（不是 data[i]）印出每個 byte（十六進位）
 *   2. 實作 find_byte()：在陣列中找第一個等於 target 的位置
 *      找到回傳索引（0-based），找不到回傳 -1
 */

#include <stdint.h>
#include <stdio.h>

/* TODO: 實作 find_byte，用指標走訪陣列 */
int find_byte(const uint8_t *data, uint32_t len, uint8_t target) {
    for (const uint8_t *p = data; p < data + len; p++){
        if (*p == target){
            return p - data;
        }
    }
    return 0;
}

int main(void) {
    uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    uint32_t len = sizeof(data);

    /* TODO: 用指標算術印出每個 byte，格式：0xAA 0xBB 0xCC 0xDD 0xEE */
    /* 提示：uint8_t *p = data; 然後用 p++ */
    for (const uint8_t *p = data; p < data + len; p++){
        printf("0x%02X ", *p);
    }


    printf("\n");

    printf("find 0xCC: index=%d\n", find_byte(data, len, 0xCC));
    printf("find 0xFF: index=%d\n", find_byte(data, len, 0xFF));

    return 0;
}
