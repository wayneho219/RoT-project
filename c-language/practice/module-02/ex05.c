/*
 * 練習 02-05：Ring Buffer
 *
 * 情境：
 *   RoT 的 UART 日誌模組需要一個環形緩衝區（ring buffer）：
 *   寫入端持續放入 byte，讀取端按順序取出。
 *   容量固定為 8 bytes，滿了拒絕寫入。
 *
 * 任務：
 *   定義 RingBuf struct，包含：
 *     buf[8]  : uint8_t 陣列
 *     head    : 下一個寫入位置（uint8_t）
 *     tail    : 下一個讀取位置（uint8_t）
 *     count   : 目前存放的 byte 數（uint8_t）
 *
 *   實作：
 *     int     rb_write(RingBuf *rb, uint8_t byte)
 *       → 寫入成功回傳 1，已滿回傳 0
 *
 *     int     rb_read(RingBuf *rb, uint8_t *out)
 *       → 讀出並存入 *out，成功回傳 1，已空回傳 0
 *
 *     uint8_t rb_count(const RingBuf *rb)
 *       → 回傳目前存放的 byte 數
 *
 * 提示：
 *   head = (head + 1) % RB_SIZE → 到達尾端時自動繞回開頭
 *   count 用來判斷「滿」(count == RB_SIZE) 和「空」(count == 0)
 */

#include <stdint.h>
#include <stdio.h>

#define RB_SIZE 8

/* TODO: 定義 RingBuf struct */
typedef struct{
    uint8_t buf[RB_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} RingBuf;    
/* TODO: 實作 rb_count */
uint8_t rb_count(const RingBuf *rb){
    return rb->count;
}
/* TODO: 實作 rb_write */
int rb_write(RingBuf *rb, uint8_t byte){
    if (rb_count(rb) == RB_SIZE){
        return 0;
    }
    rb->buf[rb->head] = byte;
    rb->head = (rb->head + 1) % RB_SIZE;
    rb->count++;
    return 1;
}    
/* TODO: 實作 rb_read */
int rb_read(RingBuf *rb, uint8_t *out){
    if  (rb_count(rb) == 0){
        return 0;
    }
    *out = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) % RB_SIZE;
    rb->count--;
    return 1;
}

int main(void) {
    RingBuf rb = {0};

    rb_write(&rb, 0xAA);
    rb_write(&rb, 0xBB);
    rb_write(&rb, 0xCC);
    printf("after 3 writes: count=%u\n", rb_count(&rb));

    uint8_t out;
    rb_read(&rb, &out);
    printf("read: 0x%02X count=%u\n", out, rb_count(&rb));
    rb_read(&rb, &out);
    printf("read: 0x%02X count=%u\n", out, rb_count(&rb));

    rb_write(&rb, 0x11);
    rb_write(&rb, 0x22);
    rb_write(&rb, 0x33);
    rb_write(&rb, 0x44);
    rb_write(&rb, 0x55);
    rb_write(&rb, 0x66);
    rb_write(&rb, 0x77);
    printf("after 7 writes: count=%u\n", rb_count(&rb));

    int ok = rb_write(&rb, 0x88);
    printf("write when full: ok=%d\n", ok);

    printf("read all:");
    while (rb_count(&rb) > 0) {
        rb_read(&rb, &out);
        printf(" 0x%02X", out);
    }
    printf("\n");
    printf("count=%u\n", rb_count(&rb));

    return 0;
}
