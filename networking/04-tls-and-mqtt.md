---
tags: [networking, tls, mqtt, ota, security]
topic: networking
---

# 網路 Module 4：TLS 與 MQTT（OTA 安全）

## TLS（Transport Layer Security）

TLS 是 HTTPS 背後的加密協定，也是嵌入式裝置安全通訊的基礎。

### TLS 的三個目標

```
1. 機密性（Confidentiality）：第三方看不到資料內容
2. 完整性（Integrity）：資料不能被竄改
3. 身份驗證（Authentication）：確認對方是誰（防冒充）
```

**cipher suite（加密套件）是什麼：** TLS 連線要用哪些加密演算法的組合。例如 `TLS_AES_256_GCM_SHA384` 代表用 AES-256-GCM 加密、SHA-384 做 MAC。TLS 握手時 client 列出自己支援的 suite，server 從中選一個。

### TLS 1.3 握手流程

```
Client (IoT device)             Server (OTA Server)
     │                                │
     │  ClientHello                   │
     │  ─── supported cipher suites ──▶   │
     │  ─── key share (ECDH public key) ──▶│
     │                                │
     │         ServerHello            │
     │  ◀── chosen cipher suite ────  │
     │  ◀── key share (server pubkey) │
     │  ◀── Certificate (X.509) ────  │
     │  ◀── CertificateVerify ──────  │
     │  ◀── Finished (MAC) ─────────  │
     │                                │
     │  [Both derive shared secret via ECDH]
     │  [Session keys derived]        │
     │                                │
     │  Finished (MAC)                │
     │  ───────────────────────────▶  │
     │                                │
     │  ====== Encrypted channel ======
     │  Application Data (encrypted) ──▶  │
     │  ◀─ Application Data (encrypted) ──│
```

### 為什麼用 ECDH 而不是 RSA 交換 key

**ECDH（Elliptic Curve Diffie-Hellman）是什麼：** 一種讓兩方在不安全的通道上，不傳送 secret 本身，就能協商出相同的 shared secret 的演算法。類比：兩人各自混入一種顏色，交換混色後的結果，各自再加入自己的顏色，最終得到相同的混合色，但旁觀者只看到交換過程中的中間色，無法還原最終色。

TLS 1.3 移除了 RSA key exchange，因為：
```
RSA key exchange（舊方式）：
  攻擃者錄下所有流量
  若未來私鑰洩漏 → 可解密所有歷史流量（沒有 Forward Secrecy）

ECDH（Diffie-Hellman）：
  每次連線用新的隨機 key pair
  Session key 不依賴私鑰 → 私鑰洩漏不影響過去的流量
  = Perfect Forward Secrecy（PFS）
```

### 嵌入式 TLS（mbedTLS）

```c
// 連接 OTA Server 並下載 firmware（精簡版）
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"

// CA 憑證（用來驗證 Server 的憑證）
// 嵌入在 firmware 中（而不是從外部下載，防中間人攻擊）
extern const unsigned char ca_cert_pem[];
extern const size_t ca_cert_len;

int ota_download(const char *host, const char *path,
                 uint8_t *buf, size_t buf_len) {
    mbedtls_net_context net;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt ca_cert;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    // 初始化
    mbedtls_net_init(&net);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_x509_crt_init(&ca_cert);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    // 載入 CA 憑證
    mbedtls_x509_crt_parse(&ca_cert, ca_cert_pem, ca_cert_len);

    // 設定 TLS（client 模式，驗證 server 憑證）
    mbedtls_ssl_config_defaults(&conf,
        MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT);

    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&conf, &ca_cert, NULL);

    // 連線
    mbedtls_net_connect(&net, host, "443", MBEDTLS_NET_PROTO_TCP);
    mbedtls_ssl_setup(&ssl, &conf);
    mbedtls_ssl_set_hostname(&ssl, host);  // SNI
    mbedtls_ssl_set_bio(&ssl, &net,
                        mbedtls_net_send, mbedtls_net_recv, NULL);

    // TLS 握手
    mbedtls_ssl_handshake(&ssl);

    // 發送 HTTP GET
    char req[256];
    snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
        path, host);
    mbedtls_ssl_write(&ssl, (unsigned char *)req, strlen(req));

    // 讀取回應
    int n = mbedtls_ssl_read(&ssl, buf, buf_len);

    mbedtls_ssl_close_notify(&ssl);
    // 清理...
    return n;
}
```

---

## MQTT（Message Queuing Telemetry Transport）

MQTT 是 IoT 裝置的標準訊息協定，設計為低頻寬、低功耗。

### 核心概念

Publish/Subscribe 模式（和 Kafka 類似）：不需要直連，裝置和後端不需要知道對方的 IP；非同步，發布者不等接收者回應。

```
Device (Publisher)        Broker (MQTT Server)        Backend (Subscriber)
    │  PUBLISH             │                               │
    │  topic: rot/status   │                               │
    │  payload: {"ok":1}   │                               │
    │─────────────────────▶│                               │
    │                      │  PUBLISH                      │
    │                      │─────────────────────────────▶ │
    │                      │  (all subscribers of rot/# receive this)
```

### MQTT Topic 設計

```
good topic 設計：
  devices/{device_id}/rot/status     rot 狀態
  devices/{device_id}/fw/version     firmware 版本
  devices/{device_id}/ota/request    請求 OTA 更新
  devices/{device_id}/ota/response   OTA 更新結果

wildcard：
  devices/+/rot/status    + = 單層萬用字元（一個裝置的 rot status）
  devices/#               # = 多層萬用字元（所有裝置的所有訊息）
```

### QoS 等級

```
QoS 0  至多一次  Fire-and-forget，可能遺失，最低開銷
QoS 1  至少一次  保證到達，但可能重複（ACK 機制）
QoS 2  恰好一次  保證到達且不重複（四次握手，最高開銷）

OTA 命令建議 QoS 1（保證到達，接收端做冪等處理）
狀態上報可用 QoS 0（偶爾漏一次無所謂）
```

### MQTT over TLS

```bash
# 連接 AWS IoT Core（MQTT over TLS 8883）
mosquitto_sub \
  --cafile root_ca.pem \
  --cert device_cert.pem \
  --key device_key.pem \
  -h xxxxxxxx.iot.ap-northeast-1.amazonaws.com \
  -p 8883 \
  -t "devices/my-device/rot/status"
```

---

## OTA 更新安全設計

結合 TLS + MQTT + RoT 的完整 OTA 流程：

```
1. 裝置啟動，M33 驗證 firmware 通過
2. Linux 啟動，rot_ipc driver 確認 M33 狀態 = OK
3. OTA agent（Linux 程式）透過 MQTT 訂閱 OTA channel
4. 後端發布新版本通知（包含 firmware URL 和版本號）

5. OTA agent 評估：
   - 新版本 > 當前版本？
   - 磁碟空間足夠？
   
6. 透過 HTTPS（TLS 1.3）下載 signed firmware
   - Server 憑證由嵌入 firmware 中的 CA cert 驗證
   - 連線過程加密（防中間人）

7. 下載完成後：
   - 計算 SHA-256 比對（完整性）
   - 通知 M33（透過 IPC）準備更新

8. M33 驗證新 firmware 的 ECDSA 簽章
   - 通過 → 寫入 microSD（fip partition）
   - 失敗 → 拒絕，保持原版本

9. M33 更新 OTP anti-rollback counter（不可逆）
10. 重啟裝置 → M33 驗證新版本，A35 啟動
```

---

## 憑證 vs Pre-Shared Key（PSK）

嵌入式 TLS 有兩種身份驗證方式：

```
憑證方式（X.509，推薦）：
  每個裝置有唯一的憑證（Device Certificate）
  由裝置的私鑰簽署的 TLS Client Hello → Server 可以識別是哪台裝置
  私鑰存在 M33 Secure Storage，不離開裝置
  
PSK 方式（Pre-Shared Key）：
  裝置和 Server 事先共享一個 secret
  不需要非對稱加密（省 CPU，適合極低功耗）
  缺點：所有裝置共用同一個 PSK → 一台被攻破全都完了
       （或每台不同 PSK → 管理複雜）
```

---

## 面試常考

| 問題 | 要點 |
|------|------|
| HTTPS 和 HTTP 的差別？ | TLS 層加密，防竊聽和竄改 |
| TLS 1.3 和 1.2 的主要差別？ | 握手減少 1 RTT；移除舊算法（RSA key exchange, CBC, MD5/SHA1）|
| Perfect Forward Secrecy 是什麼？ | 每次 session 用不同 key，私鑰洩漏不影響過去流量 |
| MQTT 和 HTTP 的差別？ | MQTT 是持久連線 + pub/sub；HTTP 是請求/回應，適合大量裝置 |
| 中間人攻擊（MITM）怎麼防？ | 驗證 Server 憑證（Certificate Pinning 更強）|
| Certificate Pinning 是什麼？ | 把 Server 的 public key hash 嵌進程式，不信任 OS 的 CA store |
| OTA 下載完不驗章會怎樣？ | MITM 可以替換 firmware，繞過 Secure Boot（下載和驗章都要做）|
