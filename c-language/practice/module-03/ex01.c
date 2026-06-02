/*
 * 練習 03-01：基本 bit 操作
 *
 * 任務：
 *   實作以下四個函式，只能用 &, |, ^, ~, <<, >> 運算子
 *   不可用 if 判斷式（除了 check_bit 的回傳）
 */

#include <stdint.h>
#include <stdio.h>

/* TODO: 設定 bit n 為 1 */
uint32_t set_bit(uint32_t reg, uint8_t n) {
    return reg | (1 << n);
}

/* TODO: 清除 bit n 為 0 */
uint32_t clear_bit(uint32_t reg, uint8_t n) {
    return reg & ~(1 << n);
}

/* TODO: 切換 bit n（0→1，1→0）*/
uint32_t toggle_bit(uint32_t reg, uint8_t n) {
    return reg ^ (1 << n);
}

/* TODO: 回傳 bit n 的值（0 或 1）*/
uint8_t check_bit(uint32_t reg, uint8_t n) {
    return (reg >> n) & 1;
}

int main(void) {
    uint32_t reg = 0b00000000;

    reg = set_bit(reg, 3);
    printf("set   bit3: 0x%02X\n", reg);   // 0x08

    reg = set_bit(reg, 7);
    printf("set   bit7: 0x%02X\n", reg);   // 0x88

    reg = clear_bit(reg, 3);
    printf("clear bit3: 0x%02X\n", reg);   // 0x80

    reg = toggle_bit(reg, 7);
    printf("toggle bit7: 0x%02X\n", reg);  // 0x00

    reg = 0b10101010;
    printf("bit1=%u bit2=%u bit3=%u\n",
           check_bit(reg, 1),
           check_bit(reg, 2),
           check_bit(reg, 3));             // 1 0 1

    return 0;
}
