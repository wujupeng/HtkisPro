# 中继与 NAT 穿透指南

## 概述

HtkisPro v2.1 提供 NAT 穿透与反向连接能力，使 NAT 后的代理服务器具备外网可达性。

## NAT 穿透

### STUN 协议
实现 RFC 5389 STUN Binding Request/Response，用于探测 NAT 类型和公网映射地址。

### NAT 类型
| 类型 | 说明 | 穿透建议 |
|------|------|----------|
| Full Cone | 所有探测返回相同映射地址 | 可直接穿透 |
| Restricted | 映射地址随源端口变化 | 可直接穿透 |
| Port Restricted | 映射端口受限 | 可直接穿透 |
| Symmetric | 每次探测映射地址不同 | 需使用中继模式 |

### 配置
在 GUI 的 **Relay/NAT** Tab → **NAT** 子面板中：
1. 勾选 **Enabled**
2. 输入 **STUN Server**（如 `stun.l.google.com`）
3. 设置 **STUN Port**（默认 3478）
4. 设置 **Keepalive** 间隔（默认 20 秒）
5. 点击 **Probe Now** 立即探测

### 保活
启用后每 `keepaliveSeconds` 秒自动发送 STUN Binding Request 刷新 NAT 映射。

## 中继隧道

### 架构
```
外部客户端 → 公网中继服务器 ← TLS隧道 ← NAT后HtkisPro
```

NAT 后的 HtkisPro 主动向公网中继服务器发起 TLS 连接，建立反向隧道。外部客户端通过中继服务器访问 HtkisPro 的代理服务。

### 配置
在 GUI 的 **Relay/NAT** Tab → **Relay** 子面板中：
1. 输入 **Server Host**（中继服务器地址）
2. 设置 **Server Port**（默认 443）
3. 输入 **Node ID**（节点标识）
4. 点击 **Add Node**

### 隧道特性
- **TLS 加密**：中继隧道必须 TLS 加密
- **认证**：节点 ID + 认证令牌哈希
- **保活心跳**：每 30 秒发送一次
- **自动重连**：指数退避（初始 200ms，最大 2000ms），10 秒内发起首次重连
- **多节点**：支持多个中继节点，按优先级选择

### 节点状态
| 状态 | 说明 |
|------|------|
| Disconnected | 未连接 |
| Connecting | TCP 连接中 |
| Authenticating | 认证中 |
| Active | 隧道活跃 |
| Reconnecting | 重连中 |

### 凭证凭证保护
认证凭证以哈希+盐方式存储（与用户密码同等保护），不明文保存。

## 请求路由

经中继的代理请求通过 `RelaySessionRouter` 路由：
1. 从中继隧道接收外部客户端请求
2. 路由至对应协议 handler
3. 响应经隧道返回外部客户端
4. 多中继节点按优先级转发

## GUI 告警
- 所有中继离线时 GUI 告警
- NAT 类型为 Symmetric 时提示需使用中继模式
- 中继隧道状态实时展示