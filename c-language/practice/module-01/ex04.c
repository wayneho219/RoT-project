/*
 * 練習 01-04：Firmware Manifest（進階）
 *
 * 情境：
 *   RoT 從 flash 讀到一份 firmware manifest，描述這個 firmware 的基本資訊。
 *   你需要設計資料結構並實作序列化/反序列化。
 *
 * Manifest 格式（共 12 bytes，依序排列）：
 *   byte  0      : magic（固定為 0xA5）
 *   byte  1      : version_major
 *   byte  2      : version_minor
 *   byte  3      : flags（bit 0 = debug build, bit 1 = signed, bit 7 = production）
 *   byte  4–7    : image_size（uint32_t，little-endian）
 *   byte  8–11   : crc32（uint32_t，little-endian，這裡直接給你填）
 *
 * 任務：
 *   1. 定義 FirmwareManifest struct（用固定寬度型別）
 *
 *   2. 實作 manifest_serialize()
 *      把 struct 轉成 12 bytes 的陣列（依照上面格式）
 *
 *   3. 實作 manifest_deserialize()
 *      把 12 bytes 陣列還原成 struct
 *
 *   4. 實作 manifest_is_valid()
 *      回傳 1 若：magic == 0xA5 且 image_size > 0
 *      回傳 0 否則
 *
 * 注意：
 *   - little-endian 表示 uint32_t 的低位 byte 先存
 *     例如 0x00010200 → bytes: [0x00, 0x02, 0x01, 0x00]
 *   - 不可用 memcpy 直接複製整個 struct（struct 可能有 padding）
 *     要逐欄位手動處理
 */

#include <stdint.h>
#include <stdio.h>

/* TODO: 定義 FirmwareManifest */
typedef struct{
    uint8_t magic;
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t flags;
    uint32_t image_size;
    uint32_t crc32;
} FirmwareManifest;

/* TODO: 實作 manifest_serialize
 *   將 manifest 序列化到 out[12]
 */
void manifest_serialize(const FirmwareManifest *m, uint8_t out[12]) {
    out[0] = m -> magic;
    out[1] = m -> version_major;
    out[2] = m -> version_minor;
    out[3] = m -> flags;
    out[4] = (uint8_t)(m -> image_size & 0xFF);
    out[5] = (uint8_t)((m -> image_size >> 8) & 0xFF);
    out[6] = (uint8_t)((m -> image_size >> 16) & 0xFF);
    out[7] = (uint8_t)((m -> image_size >> 24) & 0xFF);
    out[8] = (uint8_t)(m -> crc32 & 0xFF);
    out[9] = (uint8_t)((m -> crc32 >> 8) & 0xFF);
    out[10] = (uint8_t)((m -> crc32 >> 16) & 0xFF);
    out[11] = (uint8_t)((m -> crc32 >> 24) & 0xFF);
}

/* TODO: 實作 manifest_deserialize
 *   從 data[12] 還原成 FirmwareManifest
 */
FirmwareManifest manifest_deserialize(const uint8_t data[12]) {
    FirmwareManifest m;
    m.magic = data[0];
    m.version_major = data[1];
    m.version_minor = data[2];
    m.flags = data[3];
    m.image_size = (data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24));
    m.crc32 = (data[8] | (data[9] << 8) | (data[10] << 16) | (data[11] << 24));
    return m;
}

/* TODO: 實作 manifest_is_valid */
int manifest_is_valid(const FirmwareManifest *m) {
    if(m -> magic == 0xA5 && m -> image_size > 0) {
        return 1;
    } else {
        return 0;
    }
}

int main(void) {
    FirmwareManifest original = {
        .magic         = 0xA5,
        .version_major = 2,
        .version_minor = 1,
        .flags         = 0x82,        /* bit 1 + bit 7 */
        .image_size    = 0x00018000,  /* 96KB */
        .crc32         = 0xDEADBEEF,
    };

    /* 序列化 */
    uint8_t buf[12];
    manifest_serialize(&original, buf);

    /* 印出 bytes（用於驗證格式正確性）*/
    printf("serialized:");
    for (int i = 0; i < 12; i++) {
        printf(" %02X", buf[i]);
    }
    printf("\n");

    /* 反序列化 */
    FirmwareManifest restored = manifest_deserialize(buf);

    /* 驗證欄位還原正確 */
    printf("magic=0x%02X\n", restored.magic);
    printf("version=%u.%u\n", restored.version_major, restored.version_minor);
    printf("flags=0x%02X\n", restored.flags);
    printf("image_size=0x%08X\n", restored.image_size);
    printf("crc32=0x%08X\n", restored.crc32);
    printf("valid=%d\n", manifest_is_valid(&restored));

    /* 測試無效的 manifest */
    FirmwareManifest bad = {.magic = 0x00, .image_size = 0};
    printf("bad_valid=%d\n", manifest_is_valid(&bad));

    return 0;
}
