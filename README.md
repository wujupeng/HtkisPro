<div align="center">

# HtkisPro

**Multi-Protocol Proxy Server with GUI**

[![Platform](https://img.shields.io/badge/platform-Windows-blue.svg)](https://github.com/wujupeng/HtkisPro)
[![License](https://img.shields.io/badge/license-Private-red.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/std/the-standard)

A lightweight, secure proxy server with user authentication, real-time logging, and a modern dark-themed GUI. Supports HTTP CONNECT, SOCKS4/4a, SOCKS5 TCP/UDP, and WebSocket protocols on a single port.

</div>

---

## Features

- **Multi-Protocol** - Auto-detects HTTP CONNECT, SOCKS4/4a, SOCKS5 TCP/UDP, and WebSocket on the same port
- **User Authentication** - SHA-256 + salt password hashing, configurable expiration, brute-force protection
- **IP Access Control** - Whitelist/blacklist with CIDR notation, IPv4/IPv6 dual-stack
- **Rate Limiting** - Global and per-user bandwidth limits with 10% burst capacity
- **Connection Pool** - TCP connection reuse with LRU eviction and health check
- **DNS Cache** - LRU + TTL based DNS resolution cache with backup DNS support
- **QoS Scheduling** - Priority-based session scheduling
- **Real-Time Stats** - Global/per-user traffic statistics, active session monitoring
- **Modern GUI** - Dark-themed ImGui interface with config, stats, and log panels
- **System Tray** - Minimize to tray on window close; exit only via tray menu
- **Crash Handler** - Automatic minidump generation on crash
- **Multi-Threaded Engine** - Dual io_context with N worker threads (hardware concurrency)
- **Multi-Language** - 6 languages: English, 简体中文, Tiếng Việt, ไทย, Español, Magyar
- **Static Linking** - Single exe deployment, no external DLL dependencies

## Quick Start

### Download

Download the latest release from [Releases](https://github.com/wujupeng/HtkisPro/releases).

### Usage

1. Run `proxy_server.exe`
2. Set the proxy port in the left panel, click **Start Service**
3. Add users in the **Users** tab (username, password, validity days)
4. Configure proxy in your browser or client:
   - **Address**: Server IP + Port
   - **Type**: HTTP, SOCKS4, SOCKS5, or WebSocket
   - **Auth**: Username and Password
5. Optional: Configure IP ACL, rate limits, connection pool, DNS cache in the **Config** tab
6. Monitor traffic and sessions in the **Stats** tab

### Configuration

Configuration is stored in `config.json` (auto-created on first run). All settings can be changed at runtime via the GUI Config panel. See [docs/config_guide.md](docs/config_guide.md) for details.

### User Storage

Users are stored in `users.json` with SHA-256 + salt hashing. Legacy `users.txt` format is auto-migrated on startup.

### System Tray

- Click window **X** button → Hide to system tray (service keeps running)
- Double-click tray icon → Show main window
- Right-click tray icon → Show window / Exit program

## Build from Source

### Prerequisites

- CMake 3.14+
- Visual Studio 2022 (MSVC, with C++17 support)
- Git (for FetchContent dependencies)

Dependencies are automatically downloaded via CMake FetchContent:
- [asio](https://github.com/chriskohlhoff/asio) (standalone)
- [GLFW](https://github.com/glfw/glfw) 3.4
- [Dear ImGui](https://github.com/ocornut/imgui) v1.91.6
- [nlohmann/json](https://github.com/nlohmann/json) v3.11.3 (included in third_party/)

### Build

```bash
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Output: `build/Release/proxy_server.exe` (statically linked, ~3MB)

### Build with Tests

```bash
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build --config Release
```

## Project Structure

```
src/
├── server/
│   ├── Server.cpp/h              # Core proxy engine (multi-threaded, multi-protocol)
│   ├── GuiMain.cpp               # ImGui GUI with tray, config, stats panels
│   ├── UserManager.cpp/h         # User auth (SHA-256+salt, JSON storage)
│   ├── Logger.h                  # Thread-safe async logging
│   ├── IpAcl.cpp/h               # IP whitelist/blacklist (CIDR, IPv4/IPv6)
│   ├── Authenticator.cpp/h       # Unified auth entry point
│   ├── AuthFailureTracker.cpp/h  # Brute-force protection (sliding window)
│   ├── DeadlineTimer.cpp/h       # Connection timeout management
│   ├── DnsCache.cpp/h            # DNS cache (LRU + TTL)
│   ├── ReconnectPolicy.cpp/h     # Exponential backoff reconnection
│   ├── ConnectionPool.cpp/h      # TCP connection pool (LRU + health check)
│   ├── ProtocolDetector.cpp/h    # First-byte protocol auto-detection
│   ├── UdpRelaySession.cpp/h     # SOCKS5 UDP ASSOCIATE relay
│   ├── WebSocketFrame.cpp/h      # WebSocket frame encode/decode
│   ├── WebSocketHandler.cpp/h    # WebSocket Upgrade handshake
│   ├── TokenBucket.cpp/h         # Token bucket rate limiter
│   ├── RateLimiter.cpp/h         # Global + per-user rate limiter
│   ├── QosScheduler.cpp/h        # Priority-based QoS scheduler
│   ├── StatsCollector.cpp/h      # Traffic statistics aggregator
│   └── resource.h/rc             # Windows resources (embedded icon)
├── common/
│   ├── Config.cpp/h              # Configuration data model
│   ├── ConfigManager.cpp/h       # Config manager (JSON, atomic snapshot, hot reload)
│   ├── HashUtil.cpp/h            # SHA-256 + salt (Windows CNG)
│   ├── CrashHandler.cpp/h        # Minidump crash handler
│   └── Localization.cpp/h        # Multi-language support (6 languages)
├── third_party/
│   └── nlohmann/json.hpp         # JSON library (v3.11.3)
└── tests/
    └── unit/                     # GoogleTest unit tests (90 tests)
```

## Supported Protocols

| Protocol | Status | Description |
|----------|--------|-------------|
| HTTP CONNECT | ✅ | HTTP proxy tunneling |
| SOCKS4 | ✅ | SOCKS4 with IP address |
| SOCKS4a | ✅ | SOCKS4 with domain name |
| SOCKS5 TCP | ✅ | SOCKS5 CONNECT (IPv4/Domain/IPv6) |
| SOCKS5 UDP | ✅ | SOCKS5 UDP ASSOCIATE |
| WebSocket | ✅ | WS tunnel (Upgrade handshake) |
| HTTPS | ⏳ | TLS proxy (requires OpenSSL, planned) |

See [docs/protocol_guide.md](docs/protocol_guide.md) for protocol details.

## Log Format

Full session lifecycle logging:

```
[CONN]   New connection from 192.168.2.40:50506
[SOCKS5] Auth success user=hunt from=192.168.2.40:50506
[SOCKS5] CONNECT smtp.example.com:465 user=hunt from=192.168.2.40:50506
[DNS]    Resolving smtp.example.com ...
[DNS]    Resolved smtp.example.com -> 2 endpoint(s)
[SOCKS5] Tunnel established -> smtp.example.com:465 user=hunt remote=10.0.0.1:465
[CLOSE]  192.168.2.40:50506 user=hunt target=smtp.example.com:465 reason=client disconnect up=1.2 KB down=8.5 KB duration=2m 35s
```

## Supported Languages

| Language | Name |
|----------|------|
| English | English |
| Chinese | 简体中文 |
| Vietnamese | Tiếng Việt |
| Thai | ไทย |
| Spanish | Español (México) |
| Hungarian | Magyar |

## License

Private - All rights reserved.
