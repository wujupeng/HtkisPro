# Protocol Support Guide

HtkisPro supports multiple proxy protocols on a single port. The protocol is auto-detected from the first bytes of the client connection.

## Protocol Auto-Detection

The server uses the first byte(s) to determine the protocol:

| First Byte(s) | Protocol | Notes |
|---------------|----------|-------|
| `0x05` | SOCKS5 | SOCKS5 TCP/UDP (command determined later) |
| `0x04` | SOCKS4/4a | SOCKS4 or SOCKS4a (domain in IP field) |
| `0x16` | HTTPS/TLS | TLS handshake |
| `CONNECT `, `GET `, `POST `, etc. | HTTP | HTTP proxy or WebSocket |
| HTTP + `Upgrade: websocket` | WebSocket | WebSocket tunnel |

Detection buffer limit: 4KB. If protocol cannot be determined within 4KB, connection is closed.

## HTTP CONNECT

Standard HTTP proxy tunneling for HTTPS traffic.

```
Client → Proxy: CONNECT example.com:443 HTTP/1.1\r\nHost: example.com\r\n\r\n
Proxy → Client: HTTP/1.1 200 Connection Established\r\n\r\n
```

- Authentication: `Proxy-Authorization: Basic base64(user:pass)`
- Supports IPv4 and domain targets

## SOCKS4 / SOCKS4a

Legacy SOCKS protocol. SOCKS4a adds domain name support.

```
Client → Proxy: VER(0x04) CMD(0x01=CONNECT) DSTPORT(2 bytes) DSTIP(4 bytes) USERID \0
```

- SOCKS4: DSTIP is a 4-byte IPv4 address
- SOCKS4a: DSTIP is 0.0.0.x (x≠0), followed by domain name after USERID
- Response: VN(0x00) CODE(0x5A=success) DSTPORT DSTIP

## SOCKS5 TCP CONNECT

Full SOCKS5 with authentication and IPv4/Domain/IPv6 address types.

```
Client → Proxy: VER(0x05) NMETHODS METHODS...
Proxy → Client: VER(0x05) METHOD(0x02=username/password)
Client → Proxy: VER(0x01) ULEN UNAME PLEN PASSWD
Proxy → Client: VER(0x01) STATUS(0x00=success)
Client → Proxy: VER(0x05) CMD(0x01=CONNECT) RSV ATYP DST.ADDR DST.PORT
Proxy → Client: VER(0x05) REP(0x00=success) RSV ATYP BND.ADDR BND.PORT
```

ATYP values:
- `0x01`: IPv4 (4 bytes)
- `0x03`: Domain (1 byte length + domain)
- `0x04`: IPv6 (16 bytes)

## SOCKS5 UDP ASSOCIATE

UDP relay for datagram proxying.

```
Client → Proxy (TCP): VER(0x05) CMD(0x03=UDP ASSOCIATE) RSV ATYP DST.ADDR DST.PORT
Proxy → Client (TCP): VER(0x05) REP(0x00) RSV ATYP BND.ADDR BND.PORT
```

After association, UDP datagrams are relayed through the allocated UDP port with SOCKS5 UDP header:

```
RSV(0x000) FRAG(0x00) ATYP DST.ADDR DST.PORT DATA
```

## WebSocket Tunnel

WebSocket-based proxy tunnel for HTTP-restricted environments.

```
Client → Proxy: GET /ws HTTP/1.1\r\nUpgrade: websocket\r\nSec-WebSocket-Key: ...\r\n\r\n
Proxy → Client: HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nSec-WebSocket-Accept: ...\r\n\r\n
```

After Upgrade, WebSocket frames carry proxy protocol data (SOCKS5 or HTTP CONNECT).

Frame support:
- Text/Binary frames
- Ping/Pong keepalive
- Close frame
- Masked and unmasked frames
- Fragmentation

## HTTPS / TLS (Planned)

TLS-encrypted proxy connections. Requires OpenSSL integration.

```
Client → Proxy: TLS Handshake (0x16...)
Proxy → Client: TLS Handshake
... TLS-encrypted proxy protocol (HTTP CONNECT, SOCKS5, etc.)
```

Status: Placeholder implementation. TLS requests are rejected with a warning log.

## Authentication

All protocols share the same user database. Authentication methods:

| Protocol | Auth Method |
|----------|-------------|
| HTTP CONNECT | Proxy-Authorization (Basic) |
| SOCKS4/4a | USERID field |
| SOCKS5 | Username/Password (RFC 1929) |
| WebSocket | HTTP header (before Upgrade) |
| HTTPS | TLS client cert or inner protocol auth |

Passwords are stored as SHA-256 + salt hashes. Brute-force protection: 10 failures within 60 seconds triggers a 5-minute ban.