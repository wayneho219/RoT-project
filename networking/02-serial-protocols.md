---
tags: [networking, embedded-protocols, uart, spi, i2c]
topic: networking
---

# 網路 Module 2：嵌入式串行協定（UART / SPI / I2C）

## 三種協定的定位

```
UART  → 人機互動、debug console、與其他系統通訊（最簡單）
SPI   → 高速、短距、主控一對多（NOR Flash、LCD、感測器）
I2C   → 低速、短距、多設備共線（電源管理 IC、IMU、EEPROM）
```

---

## UART（Universal Asynchronous Receiver/Transmitter）

**UART 是什麼：** 最基本的序列通訊，兩條線（TX 傳送、RX 接收），一次傳一個 bit。「非同步」指的是沒有額外的時鐘線，雙方靠事先約定的速度（Baud Rate）對齊。嵌入式開發最常用來輸出 debug log。

```
Baud Rate（鮑率）= 每秒傳幾個 bit
  115200 bps = 每秒 115200 個 bit ≈ 每秒約 11520 個字元（8 bits + stop bit）
```

### 特性

```
非同步（無時鐘線）：雙方事先約定 Baud Rate（例如 115200 bps）
全雙工：TX 和 RX 是獨立的線（可以同時收發）
點對點：一對一通訊
接腳：TX、RX（可選：RTS、CTS 硬體流量控制）
```

### 訊框格式

```
IDLE  Start  D0  D1  D2  D3  D4  D5  D6  D7  Parity  Stop
─────┐     ┌──┬──┬──┬──┬──┬──┬──┬──┬────────┬──────
     └─────┘  └──┘  └──┘  └──┘  └──┘        └──────
```
- Start bit: 低電位（1 bit）
- Data bits: 通常 8 bits，LSB 先送
- Parity bit: 可選（even/odd）
- Stop bit: 高電位（1 or 2 bits）

### STM32 UART 程式碼

```c
// 初始化
UART_HandleTypeDef huart2;
huart2.Instance = USART2;
huart2.Init.BaudRate = 115200;
huart2.Init.WordLength = UART_WORDLENGTH_8B;
huart2.Init.StopBits = UART_STOPBITS_1;
huart2.Init.Parity = UART_PARITY_NONE;
huart2.Init.Mode = UART_MODE_TX_RX;
HAL_UART_Init(&huart2);

// 傳送
const char *msg = "RoT: firmware verified\r\n";
HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

// 接收（中斷模式）
uint8_t rx_buf[64];
HAL_UART_Receive_IT(&huart2, rx_buf, 64);

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        // 處理收到的資料
        process_uart_cmd(rx_buf);
        // 重新啟動接收
        HAL_UART_Receive_IT(&huart2, rx_buf, 64);
    }
}
```

### 常見問題

```
亂碼 → Baud Rate 不匹配（雙方必須相同）
字元丟失 → 接收 buffer 溢位（改用 DMA 接收）
第一個字元丟失 → 先收再初始化（時序問題）
```

---

## SPI（Serial Peripheral Interface）

### 特性

```
同步（有 SCK 時鐘線）
全雙工：MOSI 和 MISO 同時傳輸
主從架構：Master（CPU）控制 SCK 和 CS
多從設備：每個 slave 有獨立的 CS（Chip Select）線
速度：最高可達幾十 MHz（比 I2C 快）

接腳：SCK、MOSI（Master Out Slave In）、MISO、CS（每個 slave 一條）
```

### SPI 時序（Mode 0：CPOL=0, CPHA=0）

```
CS   ─┐                                        ┌──
      └────────────────────────────────────────┘
SCK  ──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐
       └──┘  └──┘  └──┘  └──┘  └──┘  └──┘  └──
MOSI ──── D7 ──── D6 ──── D5 ─ ... ─── D0 ──────
MISO ──── Q7 ──── Q6 ──── Q5 ─ ... ─── Q0 ──────
```
- CS 拉低 → 選中 slave → 開始傳輸
- SCK 上升沿：採樣（CPHA=0）
- CS 拉高 → 結束傳輸

### SPI 四種模式

```
CPOL=0, CPHA=0 (Mode 0): SCK 空閒低，上升沿採樣  ← 最常見
CPOL=0, CPHA=1 (Mode 1): SCK 空閒低，下降沿採樣
CPOL=1, CPHA=0 (Mode 2): SCK 空閒高，下降沿採樣
CPOL=1, CPHA=1 (Mode 3): SCK 空閒高，上升沿採樣
```

### STM32 SPI 讀 NOR Flash

```c
// NOR Flash SPI 命令（MX25L256）
#define NOR_CMD_READ    0x03
#define NOR_CMD_RDSR    0x05  // Read Status Register
#define NOR_CMD_WEN     0x06  // Write Enable

// CS 控制（透過 GPIO）
#define CS_LOW()  HAL_GPIO_WritePin(SPI_CS_GPIO, SPI_CS_PIN, GPIO_PIN_RESET)
#define CS_HIGH() HAL_GPIO_WritePin(SPI_CS_GPIO, SPI_CS_PIN, GPIO_PIN_SET)

// 讀取 Flash 資料
int nor_spi_read(uint32_t addr, uint8_t *buf, uint32_t len) {
    uint8_t cmd[4] = {
        NOR_CMD_READ,
        (addr >> 16) & 0xFF,
        (addr >>  8) & 0xFF,
        (addr >>  0) & 0xFF,
    };

    CS_LOW();
    HAL_SPI_Transmit(&hspi1, cmd, 4, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi1, buf, len, HAL_MAX_DELAY);
    CS_HIGH();
    return 0;
}
```

---

## I2C（Inter-Integrated Circuit）

### 特性

```
同步（有 SCL 時鐘線）
半雙工：SDA 是雙向的
多主多從：多個設備共用兩條線（需要上拉電阻）
尋址：每個設備有 7-bit 位址（最多 128 個設備）
速度：標準 100 kHz，Fast 400 kHz，Fast-Plus 1 MHz
接腳：SCL（時鐘）、SDA（資料）—— 只需兩條線！
```

### I2C 訊框格式

```
START  ADDR(7)  R/W  ACK  DATA(8)  ACK  ...  STOP
  │   ┌────────────────────────────────────────┐
  │   │ 0x48      0    ↑   0x1A      ↑        │
  └───┘              (slave pulls low = ACK)   └──
```
- START：SDA 在 SCL 高時拉低（特殊序列）
- ADDR：7-bit 設備位址（如 0x48）
- R/W：0 = 寫入，1 = 讀取
- ACK：slave 拉低 SDA = 確認（master 拉低 = NACK = 拒絕）
- STOP：SDA 在 SCL 高時拉高

### STM32 I2C 讀取感測器

```c
// 讀取 I2C 感測器（例如溫度感測器 LM75，位址 0x48）
#define LM75_ADDR  (0x48 << 1)  // HAL 的位址要左移 1 bit

// 寫入暫存器
uint8_t reg = 0x00;  // Temperature register
HAL_I2C_Master_Transmit(&hi2c1, LM75_ADDR, &reg, 1, HAL_MAX_DELAY);

// 讀取 2 bytes
uint8_t data[2];
HAL_I2C_Master_Receive(&hi2c1, LM75_ADDR | 0x01, data, 2, HAL_MAX_DELAY);

// 解析溫度（11-bit，0.125°C/bit）
int16_t raw = ((data[0] << 8) | data[1]) >> 5;
float temp = raw * 0.125f;

// 也可以用 Mem API（更方便）
HAL_I2C_Mem_Read(&hi2c1, LM75_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, 
                  data, 2, HAL_MAX_DELAY);
```

---

## 三種協定對比

| 特性 | UART | SPI | I2C |
|------|------|-----|-----|
| 時鐘 | 無（非同步）| 有（同步）| 有（同步）|
| 接線數 | 2（TX/RX）| 4+（SCK/MOSI/MISO/CS）| 2（SCL/SDA）|
| 雙工 | 全雙工 | 全雙工 | 半雙工 |
| 多設備 | 難（需要額外硬體）| CS 線增加 | 位址尋址 |
| 速度 | 中（~5 Mbps）| 高（50+ Mbps）| 低（1 MHz max）|
| 距離 | 中（~15 m with RS-232）| 短（板內）| 短（板內）|
| 嵌入式用途 | Debug, BLE/GPS 模組 | Flash, LCD, 感測器 | PMIC, IMU, EEPROM |

### STM32MP215F 應用

```
SDMMC1 → microSD（boot source，存 firmware）
I2C1 → PMIC（電源管理 IC，控制 OTP 燒錄電壓）
USART2 → Debug console（Linux 的 /dev/ttySTM0）
USART3 → M33 和外部設備通訊（可選）
```

---

## 面試常考

| 問題 | 要點 |
|------|------|
| SPI 和 I2C 怎麼選？ | 需要快 → SPI；接線少 / 多設備 → I2C |
| I2C 為什麼需要上拉電阻？ | I2C 是開漏輸出（Open Drain），需要外部電阻拉高 |
| UART Baud Rate 不一致會怎樣？ | 採樣點偏移 → 亂碼（framing error）|
| SPI Mode 0/1/2/3 差在哪？ | CPOL（空閒時鐘電位）和 CPHA（採樣沿）的組合 |
| I2C ACK 和 NACK 的意義？ | ACK = 收到；NACK = 拒絕或設備不存在 |
| DMA 在串行通訊中的作用？ | 傳輸資料不需要 CPU 參與，CPU 可以做其他事 |

---

## 下一步

→ [Module 3：CAN Bus 與其他協定](03-can-and-others.md)
