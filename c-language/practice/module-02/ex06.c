/*
 * 練習 02-06：TLV 串流解析
 *
 * 情境：
 *   RoT 的 flash 儲存了一段 TLV（Type-Length-Value）格式的設定資料。
 *
 *   每筆記錄格式：
 *     byte 0   : type（1=fw_version, 2=flags, 3=pubkey_hash）
 *     byte 1   : length（value 欄位的 byte 數）
 *     byte 2.. : value（length 個 bytes，little-endian uint32_t）
 *
 * 任務：
 *   實作 tlv_find()：
 *     在 buf[buf_len] 中搜尋第一個 type == target_type 的記錄，
 *     把 value 重新組成 uint32_t（little-endian）存入 *out，
 *     找到回傳 1，找不到回傳 0
 *
 * 限制：
 *   - 用指標走訪（const uint8_t *p = buf），不可用 buf[i] 下標
 *   - 每次跳過一筆記錄：p += 2 + length
 *   - 邊界檢查：p 不可超過 buf + buf_len
 *
 * 提示：
 *   p[0] = type, p[1] = length, p[2]..p[1+length] = value bytes
 *   little-endian 重組：value = p[2] | (p[3]<<8) | (p[4]<<16) | (p[5]<<24)
 */

#include <stdint.h>
#include <stdio.h>

uint32_t read_u32(const uint8_t *p){
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* TODO: 實作 tlv_find */
int tlv_find(const uint8_t *buf, uint32_t buf_len,
             uint8_t target_type, uint32_t *out) {
    for (const uint8_t *p = buf; p < buf + buf_len; p += *(p+1) + 2){
        if (*p == target_type){
            *out = read_u32(p + 2);
            return 1;
        }
    }
    return 0; 
}

int main(void) {
    /* TLV 串流：fw_version=3, flags=0x82, pubkey_hash=0xDEADBEEF */
    uint8_t stream[] = {
        0x01, 0x04, 0x03, 0x00, 0x00, 0x00,  /* type=1, len=4, value=3 */
        0x02, 0x04, 0x82, 0x00, 0x00, 0x00,  /* type=2, len=4, value=0x82 */
        0x03, 0x04, 0xEF, 0xBE, 0xAD, 0xDE,  /* type=3, len=4, value=0xDEADBEEF */
    };
    uint32_t len = sizeof(stream);

    uint32_t val;
    if (tlv_find(stream, len, 1, &val)) printf("fw_version=%u\n", val);
    if (tlv_find(stream, len, 2, &val)) printf("flags=0x%08X\n", val);
    if (tlv_find(stream, len, 3, &val)) printf("pubkey_hash=0x%08X\n", val);

    int found = tlv_find(stream, len, 99, &val);
    printf("type 99 found=%d\n", found);

    return 0;
}
