/*
 * 練習 02-04：原始 Buffer 解析
 *
 * 情境：
 *   RoT 從 flash 讀到一段原始 bytes，代表一個 firmware header。
 *   你需要用指標算術（不可使用 struct overlay）把各欄位解析出來。
 *
 * Header 格式（共 8 bytes）：
 *   byte 0     : magic（應為 0xA5）
 *   byte 1     : version
 *   byte 2–3   : flags（uint16_t，little-endian）
 *   byte 4–7   : load_address（uint32_t，little-endian）
 *
 * 任務：
 *   1. 實作 read_u16(const uint8_t *p)
 *      → 從 p 讀取 2 bytes，little-endian 組成 uint16_t 回傳
 *
 *   2. 實作 read_u32(const uint8_t *p)
 *      → 從 p 讀取 4 bytes，little-endian 組成 uint32_t 回傳
 *
 *   3. 在 main 中用指標算術（p + offset）存取各欄位，
 *      呼叫 read_u16 / read_u32 解析多 byte 欄位，印出結果
 *
 * 限制：
 *   不可定義 struct，只能用 uint8_t * 和指標算術
 */

#include <stdint.h>
#include <stdio.h>

/* TODO: 從 p 讀取 2 bytes（little-endian）組成 uint16_t */
uint16_t read_u16(const uint8_t *p) {
    return (p[0] | p[1] << 8);
}

/* TODO: 從 p 讀取 4 bytes（little-endian）組成 uint32_t */
uint32_t read_u32(const uint8_t *p) {
    return (p[0] | (p[2] << 8) | (p[3] << 16) | (p[3] << 24));
}

int main(void) {
    /* 模擬 flash 中的 firmware header（8 bytes） */
    uint8_t header[] = {
        0xA5,                    /* magic */
        0x02,                    /* version */
        0x82, 0x01,              /* flags = 0x0182，little-endian */
        0x00, 0x00, 0x00, 0x20  /* load_address = 0x20000000，little-endian */
    };

    const uint8_t *p = header;

    /* TODO: 用 p + offset 存取各欄位，印出解析結果 */
    /* 期望輸出見 ex04.expected */
    printf("magic=0x%02X\nversion=%d\nflags=0x%04X\nload_address=0x%08X\n", p[0], p[1], read_u16(p+2), read_u32(p+4));

    return 0;
}
