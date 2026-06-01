---
tags: [networking, index]
topic: networking
---

# 網路與協定學習筆記

## 本模組的定位

韌體工程師需要的網路知識，分兩塊：

| 面向 | 內容 |
|------|------|
| **OSI 模型** | 面試必考的概念框架 |
| **嵌入式協定** | 實作時真正用到的：SPI、I2C、UART、CAN、MQTT、TLS |

這個專案（RoT）主要不涉及上層網路，但 OTA 更新和 Remote Attestation 會需要 TLS。

## 閱讀順序

| 模組 | 主題 |
|------|------|
| [01-osi-model](01-osi-model.md) | OSI 7 層、TCP/IP 4 層、面試常考 |
| [02-serial-protocols](02-serial-protocols.md) | UART、SPI、I2C：嵌入式最常用的三種協定 |
| [03-can-and-others](03-can-and-others.md) | CAN Bus、USB、以太網路基礎 |
| [04-tls-and-mqtt](04-tls-and-mqtt.md) | TLS 握手、MQTT、OTA 更新安全性 |
