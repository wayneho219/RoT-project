---
tags: [networking, osi, tcp-ip]
topic: networking
---

# 網路 Module 1：OSI 模型與 TCP/IP

## OSI 7 層模型

OSI（Open Systems Interconnection）是**概念框架**，用來理解網路協定的分工。  
實際網路用的是 TCP/IP 4 層，但面試幾乎必考 OSI。

```
層級  名稱                  職責                        協定範例
────────────────────────────────────────────────────────────────
7    應用層 Application     提供應用程式的網路介面        HTTP, HTTPS, MQTT, DNS, FTP
6    表示層 Presentation     資料格式轉換、加密、壓縮      TLS/SSL（有時放第 4 層）
5    會話層 Session          管理連線的建立和結束          NetBIOS, RPC
4    傳輸層 Transport        端對端可靠傳輸、流量控制      TCP, UDP
3    網路層 Network          邏輯定址、路由               IP（IPv4/IPv6）, ICMP
2    資料鏈結層 Data Link     實體位址（MAC）、錯誤偵測    Ethernet, Wi-Fi（802.11）
1    實體層 Physical         訊號、媒介（電壓、頻率）      網路線、光纖、無線電波
```

### 記憶口訣（由下到上）

```
Please Do Not Throw Sausage Pizza Away
Physical / Data Link / Network / Transport / Session / Presentation / Application
```

---

## 各層的實際例子

### 一個 HTTPS 請求（`GET https://api.example.com/fw_version`）

```
應用層（7）：HTTP GET 請求
    │  "GET /fw_version HTTP/1.1\r\nHost: api.example.com\r\n\r\n"
    ▼
表示層（6）：TLS 加密
    │  把 HTTP 訊息加密成密文（AES-GCM）
    ▼
傳輸層（4）：TCP
    │  加入 Port 號（來源: 54321，目的: 443）
    │  加入序號（Sequence Number）確保有序
    ▼
網路層（3）：IP
    │  加入來源 IP、目的 IP
    │  決定路由（經過哪些 router）
    ▼
資料鏈結層（2）：Ethernet
    │  加入 MAC Address（封裝成 Frame）
    │  加入 CRC 錯誤檢查
    ▼
實體層（1）：電訊號
    │  0/1 轉換成電壓、光波或無線電波傳送
```

---

## TCP/IP 4 層模型

實際網路工程師用的模型（比 OSI 更接近實作）：

```
TCP/IP 4 層          對應 OSI 層
────────────────────────────────
應用層 Application   OSI 5, 6, 7
傳輸層 Transport     OSI 4
網際網路層 Internet   OSI 3
網路存取層 Network    OSI 1, 2
Access
```

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
  │  SYN（seq=x）          │
  │─────────────────────▶ │
  │                        │
  │  SYN-ACK（seq=y, ack=x+1）
  │ ◀───────────────────── │
  │                        │
  │  ACK（ack=y+1）        │
  │─────────────────────▶ │
  │                        │
  │  連線建立，開始傳資料    │
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

```
瀏覽器輸入 api.example.com
  │
  ▼ DNS 查詢（UDP port 53）
DNS Resolver
  │
  ▼ 若 cache 沒有
Root DNS Server → .com TLD Server → example.com 的 DNS Server
  │
  ▼ 回傳 IP：203.0.113.42
  │
瀏覽器連到 203.0.113.42:443
```

---

## 嵌入式常見的網路需求

在 RoT 和嵌入式 Linux 裝置中，網路通常用於：

```
OTA 更新（Over-the-Air）：
  裝置 ─── HTTPS ──▶ OTA Server
  下載新 firmware → M33 驗章 → 更新 NOR Flash

遠端驗證（Remote Attestation）：
  裝置 ─── TLS + MQTT ──▶ 驗證服務
  裝置產生含硬體簽章的 report → 服務端驗證裝置真實性

裝置管理：
  裝置 ─── MQTT ──▶ IoT Platform（AWS IoT、Azure IoT Hub）
  回報狀態、接收命令
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
