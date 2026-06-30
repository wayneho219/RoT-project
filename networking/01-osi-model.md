---
tags: [networking, osi, tcp-ip]
topic: networking
---

# 網路 Module 1：OSI 模型與 TCP/IP

## OSI 7 層模型

OSI（Open Systems Interconnection）是**概念框架**，用來理解網路協定的分工。  
實際網路用的是 TCP/IP 4 層，但面試幾乎必考 OSI。

```
┌─────────────────────────────────────────────────────────────┐
│  7  Application   │ HTTP, HTTPS, MQTT, DNS, FTP             │
├─────────────────────────────────────────────────────────────┤
│  6  Presentation  │ TLS/SSL (encryption, encoding)          │
├─────────────────────────────────────────────────────────────┤
│  5  Session       │ NetBIOS, RPC                            │
├─────────────────────────────────────────────────────────────┤
│  4  Transport     │ TCP, UDP                                │
├─────────────────────────────────────────────────────────────┤
│  3  Network       │ IP (IPv4/IPv6), ICMP                    │
├─────────────────────────────────────────────────────────────┤
│  2  Data Link     │ Ethernet, Wi-Fi (802.11)                │
├─────────────────────────────────────────────────────────────┤
│  1  Physical      │ Cable, fiber, radio waves               │
└─────────────────────────────────────────────────────────────┘
```
各層中文說明：7 應用層（提供應用程式網路介面）、6 表示層（格式轉換/加密/壓縮）、5 會話層（連線管理）、4 傳輸層（端對端可靠傳輸）、3 網路層（邏輯定址/路由）、2 資料鏈結層（MAC 位址/錯誤偵測）、1 實體層（訊號/媒介）

### 記憶口訣（由下到上）

```
Please Do Not Throw Sausage Pizza Away
Physical / Data Link / Network / Transport / Session / Presentation / Application
```

---

## 各層的實際例子

### 一個 HTTPS 請求（`GET https://api.example.com/fw_version`）

應用層（7）→ 表示層（6）→ 傳輸層（4）→ 網路層（3）→ 資料鏈結層（2）→ 實體層（1）

```
Layer 7 (Application)
    │  HTTP GET: "GET /fw_version HTTP/1.1\r\nHost: api.example.com\r\n\r\n"
    ▼
Layer 6 (Presentation) — TLS
    │  Encrypt HTTP payload with AES-GCM
    ▼
Layer 4 (Transport) — TCP
    │  Add port numbers (src: 54321, dst: 443)
    │  Add sequence number for ordering
    ▼
Layer 3 (Network) — IP
    │  Add src IP, dst IP
    │  Routing decision (which router to use)
    ▼
Layer 2 (Data Link) — Ethernet
    │  Add MAC addresses (frame encapsulation)
    │  Add CRC for error detection
    ▼
Layer 1 (Physical)
    │  Encode bits as voltage, light, or radio waves
```

---

## TCP/IP 4 層模型

實際網路工程師用的模型（比 OSI 更接近實作）：

```
┌─────────────────────┬──────────────────┐
│  TCP/IP Layer       │  Maps to OSI     │
├─────────────────────┼──────────────────┤
│  Application        │  5, 6, 7         │
│  Transport          │  4               │
│  Internet           │  3               │
│  Network Access     │  1, 2            │
└─────────────────────┴──────────────────┘
```
對應中文名：應用層、傳輸層、網際網路層、網路存取層

---

## TCP vs UDP

| | TCP | UDP |
|-|-----|-----|
| 連線 | 需要三次握手（有狀態）| 無連線（無狀態）|
| 可靠性 | 保證到達、有序 | 不保證 |
| 速度 | 較慢（有確認機制）| 較快 |
| 用途 | HTTP, FTP, SSH | DNS, 影音串流, IoT 遙測 |

### TCP 三次握手

```
Client                    Server
  │  SYN (seq=x)           │
  │─────────────────────▶ │
  │                        │
  │  SYN-ACK (seq=y,ack=x+1)
  │ ◀───────────────────── │
  │                        │
  │  ACK (ack=y+1)         │
  │─────────────────────▶ │
  │                        │
  │  [Connection established — data transfer begins]
  │
```

### TCP 四次揮手（關閉連線）

```
Client         Server
  │  FIN  ───▶ │
  │  ACK  ◀─── │
  │  FIN  ◀─── │
  │  ACK  ───▶ │
```

---

## IP 定址

### IPv4

```
格式：4 個 0–255 的數字，以 . 分隔
192.168.1.100/24

/24 = 子網路遮罩（Subnet Mask）= 255.255.255.0
  → 前 24 bits 是網路位址（192.168.1），後 8 bits 是主機位址（0–255）
  → 這個子網最多 254 個主機（.1 – .254，.0 是網路位址，.255 是廣播）
```

### 常用私有 IP 範圍

```
10.0.0.0     – 10.255.255.255    /8  (Class A)
172.16.0.0   – 172.31.255.255    /12 (Class B)
192.168.0.0  – 192.168.255.255   /16 (Class C)
127.0.0.0    – 127.255.255.255       (Loopback)
```

---

## DNS（域名解析）

輸入 `api.example.com` → DNS 查詢（UDP port 53）→ Resolver → 若 cache 沒有 → Root → TLD → NS → 回傳 IP

```
Browser: "api.example.com?"
  │
  ▼  DNS query (UDP port 53)
DNS Resolver
  │
  ▼  cache miss — recursive lookup
Root DNS Server → .com TLD Server → example.com NS
  │
  ▼  returns IP: 203.0.113.42
  │
Browser connects to 203.0.113.42:443
```

---

## 嵌入式常見的網路需求

在 RoT 和嵌入式 Linux 裝置中，網路通常用於：

OTA 更新、遠端驗證、裝置管理的通訊示意：

```
[OTA Update]
  Device ─── HTTPS ──▶ OTA Server
  download firmware → M33 verify signature → write microSD

[Remote Attestation]
  Device ─── TLS + MQTT ──▶ Attestation Service
  device generates hardware-signed report → server verifies authenticity

[Device Management]
  Device ─── MQTT ──▶ IoT Platform (AWS IoT / Azure IoT Hub)
  report status, receive commands
```

---

## 面試常考：OSI 相關

| 問題 | 要點 |
|------|------|
| OSI 第幾層處理路由？ | 第 3 層（Network / IP 層）|
| MAC Address 在哪層？ | 第 2 層（Data Link）|
| TCP 和 UDP 的差別？ | TCP 可靠有序；UDP 快但不保證到達 |
| 三次握手的目的？ | 確認雙方的收發能力，協商初始序號 |
| HTTP 和 HTTPS 的差別？ | HTTPS = HTTP + TLS（第 4/6 層加密）|
| Ping 用什麼協定？ | ICMP（網路層，不是 TCP/UDP）|
| Port 號在哪層？ | 第 4 層（Transport）|
| 為什麼有了 IP 還需要 MAC？ | IP 是邏輯位址（可變）；MAC 是實體位址（燒在網卡）。同一個子網路用 MAC 通訊（ARP 解析）|

---

## 下一步

→ [Module 2：嵌入式串行協定](02-serial-protocols.md)
