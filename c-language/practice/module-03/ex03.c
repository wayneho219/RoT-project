/*
 * 練習 03-03：簡易 firmware header 解析
 *
 * 情境：
 *   RoT 從 flash 讀到一個 32-bit 的 header word，
 *   格式如下：
 *
 *   bit [31:24] = magic (應為 0xA5)
 *   bit [23:16] = version
 *   bit [15: 8] = flags
 *   bit [ 7: 0] = checksum
 *
 * 任務：
 *   1. 實作 parse_header()，從一個 uint32_t 解析出四個欄位
 *   2. 實作 build_header()，把四個欄位組合成 uint32_t
 *   3. 驗證：build 完再 parse，結果要一致
 */

#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint8_t magic;
    uint8_t version;
    uint8_t flags;
    uint8_t checksum;
} FirmwareHeader;

/* TODO */
FirmwareHeader parse_header(uint32_t word) {
    FirmwareHeader h;
    h.checksum = word & 0xFF;
    h.flags = (word >> 8) & 0xFF;
    h.version = (word >> 16) & 0xFF;
    h.magic = (word >> 24) & 0xFF;
    return h;
}

/* TODO */
uint32_t build_header(uint8_t magic, uint8_t version,
                      uint8_t flags, uint8_t checksum) {
    /* ... */
    uint32_t word = (magic << 24 | version << 16 | flags << 8 | checksum);
    return word;
}

int main(void) {
    uint32_t raw = 0xA5031F42;
    FirmwareHeader h = parse_header(raw);

    printf("magic=0x%02X version=%u flags=0x%02X checksum=0x%02X\n",
           h.magic, h.version, h.flags, h.checksum);

    uint32_t rebuilt = build_header(h.magic, h.version, h.flags, h.checksum);
    printf("match=%u\n", rebuilt == raw ? 1 : 0);

    return 0;
}
