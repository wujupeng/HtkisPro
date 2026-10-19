# Configuration Guide

HtkisPro uses a JSON configuration file (`config.json`) for all settings. The file is auto-created with defaults on first run. All settings can be changed at runtime via the GUI Config panel.

## Configuration File

Location: Same directory as `proxy_server.exe`

### Full Example

```json
{
  "ipAcl": {
    "mode": "disabled",
    "rules": [
      {"network": "192.168.1.0/24", "isIpv6": false},
      {"network": "2001:db8::/32", "isIpv6": true}
    ]
  },
  "rateLimit": {
    "globalUpLimit": 0,
    "globalDownLimit": 0,
    "userLimits": {
      "alice": {"upLimit": 1000000, "downLimit": 2000000, "priority": 1}
    }
  },
  "connectionPool": {
    "enabled": true,
    "maxConnections": 256,
    "idleTtlSeconds": 60,
    "crossUserReuse": false
  },
  "dnsCache": {
    "enabled": true,
    "fixedTtlSeconds": 300,
    "maxEntries": 4096,
    "backupDnsServers": ["8.8.8.8", "1.1.1.1"]
  },
  "tls": {
    "enabled": false,
    "certFilePath": "",
    "keyFilePath": "",
    "minTlsVersion": "1.2"
  },
  "timeout": {
    "firstPacketSeconds": 10,
    "connectSeconds": 10,
    "idleSeconds": 300,
    "retryCount": 3,
    "retryInitialBackoffMs": 200,
    "retryMaxBackoffMs": 2000
  }
}
```

## Configuration Items

### IP ACL (`ipAcl`)

Controls which client IPs can connect to the proxy.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `mode` | string | `"disabled"` | `"disabled"`, `"whitelist"`, or `"blacklist"` |
| `rules` | array | `[]` | List of CIDR rules |
| `rules[].network` | string | - | CIDR notation (e.g., `192.168.1.0/24`) |
| `rules[].isIpv6` | bool | `false` | Whether the rule is IPv6 |

- **Disabled**: All IPs allowed
- **Whitelist**: Only listed IPs allowed
- **Blacklist**: Listed IPs denied, all others allowed
- Supports IPv4 and IPv6 dual-stack

### Rate Limit (`rateLimit`)

Bandwidth throttling with 10% burst capacity.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `globalUpLimit` | uint64 | `0` | Global upload limit (bytes/sec), 0=unlimited |
| `globalDownLimit` | uint64 | `0` | Global download limit (bytes/sec), 0=unlimited |
| `userLimits` | object | `{}` | Per-user limits |
| `userLimits[name].upLimit` | uint64 | `0` | User upload limit, 0=unlimited |
| `userLimits[name].downLimit` | uint64 | `0` | User download limit, 0=unlimited |
| `userLimits[name].priority` | int | `0` | QoS priority (higher=more bandwidth) |

### Connection Pool (`connectionPool`)

TCP connection reuse to reduce handshake overhead.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Enable connection pooling |
| `maxConnections` | uint32 | `256` | Maximum pooled connections |
| `idleTtlSeconds` | uint32 | `60` | Idle connection TTL |
| `crossUserReuse` | bool | `false` | Allow sharing connections across users |

### DNS Cache (`dnsCache`)

Caches DNS resolution results to reduce latency.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Enable DNS caching |
| `fixedTtlSeconds` | uint32 | `300` | Cache TTL (seconds) |
| `maxEntries` | uint32 | `4096` | Maximum cache entries (LRU eviction) |
| `backupDnsServers` | array | `[]` | Fallback DNS servers |

### TLS (`tls`)

TLS/HTTPS proxy support (requires OpenSSL, planned feature).

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Enable TLS |
| `certFilePath` | string | `""` | Path to TLS certificate |
| `keyFilePath` | string | `""` | Path to TLS private key |
| `minTlsVersion` | string | `"1.2"` | Minimum TLS version (`"1.0"`, `"1.1"`, `"1.2"`, `"1.3"`) |

### Timeout (`timeout`)

Connection timeout and retry settings.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `firstPacketSeconds` | uint32 | `10` | Wait for first packet |
| `connectSeconds` | uint32 | `10` | Target connection timeout |
| `idleSeconds` | uint32 | `300` | Idle connection timeout |
| `retryCount` | uint32 | `3` | Reconnection attempts |
| `retryInitialBackoffMs` | uint32 | `200` | Initial backoff (ms) |
| `retryMaxBackoffMs` | uint32 | `2000` | Maximum backoff (ms) |

## Hot Reload

All configuration changes made via the GUI Config panel take effect immediately without restarting the service. Changes are saved to `config.json` automatically.

## User Storage

Users are stored in `users.json`:

```json
{
  "users": [
    {
      "username": "alice",
      "salt": "a1b2c3d4e5f6...",
      "hash": "f7e8d9c0b1a2...",
      "expireTime": "2025-12-31T23:59:59",
      "rateLimit": 0
    }
  ]
}
```

Legacy `users.txt` format (plaintext passwords) is automatically migrated to `users.json` with hashing on startup.