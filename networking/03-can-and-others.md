---
tags: [networking, can-bus, usb, ethernet]
topic: networking
---

# 網路 Module 3：CAN Bus、USB、Ethernet

## CAN Bus（Controller Area Network）

### 為什麼嵌入式工程師要懂 CAN

CAN 是汽車電子和工業控制的主流協定，如果你的履歷未來要往車用或工業靠：

```
汽車 ECU 通訊：引擎控制 ↔ ABS ↔ 儀表板（CAN）
工業設備：PLC ↔ 感測器 ↔ 驅動器（CANopen）
機器人：馬達控制（CAN FD）
```

### CAN 特性

```
2 條差分信號線（CAN_H、CAN_L），天生抗雜訊
多主架構：任何節點都可以發送（用優先級仲裁）
廣播：所有節點都能收到，每個節點自己決定要不要接受
速度：CAN 2.0 最高 1 Mbps；CAN FD 最高 8 Mbps
距離：1 Mbps 最遠 ~40 m；100 kbps 最遠 ~500 m
自動錯誤偵測和重傳
```

### CAN 訊框格式（標準幀）

```
SOF  ID(11)  RTR  IDE  r  DLC(4)  Data(0-8 bytes)  CRC(15)  ACK  EOF
 1    11       1    1   1    4      0–64 bits          15      2    7

ID = 訊息識別符（也是優先級，ID 越小越高優先）
DLC = 資料長度（0–8 bytes）
RTR = 遠端請求幀（Remote Transmission Request）
```

### 仲裁機制

```
兩個節點同時發送：
  Node A 發 ID = 0x100（binary: 001 0000 0000）
  Node B 發 ID = 0x0F0（binary: 000 1111 0000）

  CAN 線是 wired-AND：任何節點拉低就是低
  第 9 個 bit：A 發 1（高），B 發 0（低）→ 線上看到 0
  Node A 發了 1 卻看到 0 → 仲裁輸了 → 停止發送
  Node B 贏得匯流排 → 繼續發送（不需要重發）
```

### STM32 CAN 程式碼

```c
// 設定 CAN filter（只接受特定 ID）
CAN_FilterTypeDef filter;
filter.FilterBank = 0;
filter.FilterMode = CAN_FILTERMODE_IDMASK;
filter.FilterScale = CAN_FILTERSCALE_32BIT;
filter.FilterIdHigh = 0x100 << 5;  // 只接受 ID = 0x100
filter.FilterMaskIdHigh = 0x7FF << 5;
filter.FilterActivation = ENABLE;
HAL_CAN_ConfigFilter(&hcan1, &filter);

// 發送
CAN_TxHeaderTypeDef tx_hdr = {
    .StdId = 0x100,
    .IDE   = CAN_ID_STD,
    .RTR   = CAN_RTR_DATA,
    .DLC   = 4,
};
uint8_t tx_data[4] = {0x01, 0x02, 0x03, 0x04};
uint32_t tx_mailbox;
HAL_CAN_AddTxMessage(&hcan1, &tx_hdr, tx_data, &tx_mailbox);

// 接收（透過中斷）
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    CAN_RxHeaderTypeDef rx_hdr;
    uint8_t rx_data[8];
    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_hdr, rx_data);
}
```

---

## USB（Universal Serial Bus）

### USB 在嵌入式的常見用途

四種常見用途如下：

```
[DFU - Device Firmware Update]
  PC ─── USB ──▶ STM32MP215F (DFU mode)
  download firmware, write Flash

[CDC - Virtual COM Port]
  STM32 emulates COM port
  PC sees /dev/ttyUSB0 or COM3

[HID - Human Interface Device]
  device emulates keyboard / mouse / gamepad

[Mass Storage]
  device emulates USB flash drive (read/write SD card)
```

### USB 速度

```
Low Speed    1.5 Mbps   USB 1.0（HID 裝置）
Full Speed   12 Mbps    USB 1.1（CDC, HID）
High Speed   480 Mbps   USB 2.0（Mass Storage, Camera）
SuperSpeed   5 Gbps     USB 3.0
```

### USB DFU（Device Firmware Update）

STM32MP215F-DK 的 DFU 模式：

```bash
# 把 boot pins 設定到 USB DFU 模式後上電
# PC 端用 STM32CubeProgrammer

# 或用 dfu-util（Linux/Mac）
dfu-util -l                          # 列出可用裝置
dfu-util -D firmware.bin -a 0 -s 0x08000000  # 燒錄
```

---

## Ethernet（以太網路）

### 嵌入式 Linux 的 Ethernet

STM32MP215F-DK 有 GMAC（Gigabit MAC），配合外部 PHY chip：

```
CPU ─── GMAC (MAC layer) ─── MII/RGMII bus ─── PHY chip ─── RJ45
```
- MAC（Media Access Control）：軟體層（Linux 驅動）
- PHY（Physical Layer）：硬體晶片（處理電訊號）
- MII/RGMII：MAC 和 PHY 之間的介面

### Linux Ethernet 配置

```bash
# 查看網路介面
ip link show
ip addr show eth0

# 靜態 IP
ip addr add 192.168.1.100/24 dev eth0
ip route add default via 192.168.1.1

# DHCP
udhcpc -i eth0     # busybox
dhclient eth0      # 完整 Linux

# 測試連線
ping 192.168.1.1
```

### Device Tree 的 Ethernet 設定

```dts
// stm32mp215f-dk.dts
gmac0: ethernet@4c000000 {
    compatible = "st,stm32mp25-dwmac";
    reg = <0x4c000000 0x4000>;
    clocks = <&rcc ETH1MAC>;
    phy-handle = <&phy0>;
    
    mdio {
        phy0: ethernet-phy@1 {
            compatible = "ethernet-phy-id0022.1622";
            reg = <1>;
        };
    };
};
```

---

## 協定對比（嵌入式視角）

| 協定 | OSI 層 | 拓撲 | 速度 | 主要用途 |
|------|--------|------|------|---------|
| UART | 1-2 | 點對點 | ~5 Mbps | Debug, 模組通訊 |
| SPI | 1-2 | 主從 | 50+ Mbps | Flash, 感測器 |
| I2C | 1-2 | 多主多從 | 1 MHz | 配置 IC |
| CAN | 1-2 | 匯流排 | 1 Mbps | 汽車, 工業 |
| USB | 1-4 | 主從 | 480 Mbps | DFU, 偵錯 |
| Ethernet | 1-2 | 星狀 | 1 Gbps | Linux 網路 |

---

## 面試常考

| 問題 | 要點 |
|------|------|
| CAN Bus 怎麼仲裁？| Wired-AND：發 1 看到 0 就輸（低 ID 優先）|
| CAN 和 RS-485 有什麼差？ | 都是差分信號；CAN 有硬體仲裁和錯誤偵測；RS-485 沒有 |
| USB 的 4 條線分別是？ | VBUS（電源）、D-（差分負）、D+（差分正）、GND |
| Ethernet 為什麼要 PHY chip？ | MAC 做封包處理；PHY 做實體電訊號（需要特殊類比電路）|
| MDIO 是什麼？ | 管理介面，CPU 用來設定 PHY 暫存器（速度、雙工模式）|

---

## 下一步

→ [Module 4：TLS 與 MQTT（OTA 安全）](04-tls-and-mqtt.md)
