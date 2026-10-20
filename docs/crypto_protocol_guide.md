# 加密协议配置指南

## 概述

HtkisPro v2.1 支持 4 种主流加密代理协议，各自绑定独立监听端口，与主端口 65510 上的明文协议（HTTP/SOCKS5/WebSocket）共存。

## Shadowsocks

### 支持的加密方法
- `aes-256-gcm` (推荐)
- `aes-128-gcm`
- `chacha20-ietf-poly1305`

### 配置
在 GUI 的 **Shadowsocks** Tab 中：
1. 勾选 **Enabled**
2. 设置 **Port**（默认 8388）
3. 输入 **Password**
4. 选择 **Method**
5. 点击 **Apply**

### 多端口
支持多个 `(port, password, method)` 三元组独立监听，通过 `config.json` 的 `shadowsocks.ports` 数组配置。

### 密钥派生
- 主密钥：`EVP_BytesToKey(password, keyLength)` (MD5 迭代)
- 会话密钥：`HKDF-SHA1(masterKey, salt)` → 16/32 字节

## VMess

### 配置
在 GUI 的 **VMess** Tab 中：
1. 勾选 **Enabled**
2. 设置 **Port**（默认 10086）
3. 输入 **UUID**（格式：8-4-4-4-12 hex）
4. 选择 **alterId**（0 或 16）
5. 点击 **Apply**

### 协议流程
1. 客户端发送 AES-128-CFB 加密头部
2. 服务端用 `MD5(UUID)` 作为 key、`MD5(MD5(UUID)+UUID)` 作为 IV 解密
3. 解析头部：version + UUID + command + port + addrType + addr
4. UUID 校验通过后建立隧道
5. 数据通过 AEAD 分块加密传输

### alterId 限制
`alterId > 16` 不被支持，GUI 会拒绝配置。

## VLESS

### 配置
在 GUI 的 **VLESS** Tab 中：
1. 勾选 **Enabled**
2. 设置 **Port**（默认 10087）
3. 输入 **UUID**
4. 选择 **Transport**（TCP / TLS / WebSocket）
5. 点击 **Apply**

### 安全提示
- **TCP** 外层传输不加密，GUI 会显示安全风险警告
- **TLS** 外层复用 TlsContext 证书
- **WebSocket** 外层复用 WebSocketHandler

### 协议流程
1. 客户端发送明文头部（加密由外层 TLS/WS 提供）
2. 解析：version + UUID + addons + command + port + addrType + addr
3. UUID 校验通过后明文透传数据

## Trojan

### 配置
在 GUI 的 **Trojan** Tab 中：
1. 勾选 **Enabled**（需先配置 TLS 证书）
2. 设置 **Port**（默认 443）
3. 输入 **Password**
4. 设置 **TLS Cert Path** 和 **TLS Key Path**
5. 点击 **Apply**

### 前置条件
- 必须先在 **TLS** Tab 加载 TLS 证书
- 未配置证书时 Trojan 启用按钮被禁用

### 协议流程
1. TLS 握手（服务端）
2. 读取 `SHA224(password)` 十六进制字符串（56 字节）+ CRLF
3. 哈希校验通过 → 解析 SOCKS5 目标地址 → 建立隧道
4. 哈希校验失败 → 返回 HTTP 503 伪装响应（不暴露代理特征）

### 流量伪装
非 Trojan 流量接入时返回 HTTP 503 响应，使端口看起来像普通 HTTPS 服务器。

F