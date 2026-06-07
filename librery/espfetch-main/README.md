# espfetch

[![Arduino Library](https://img.shields.io/badge/Arduino-Library-blue?logo=arduino)](https://github.com/HamzaYslmn/espfetch)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![GitHub Stars](https://img.shields.io/github/stars/HamzaYslmn/espfetch?style=social)](https://github.com/HamzaYslmn/espfetch)

Neofetch-style system info + Python-style logger for ESP32/ESP8266.

## What You Get

```
     ||||||||         espfetch@esp32-D03FE0
  .--+------+--.      ----------------------------
  |            |      Chip      ESP32-D0WD-V3 rev300
  |   ESP-32   |      Cores     2 @ 240 MHz
  |            |      SDK       v5.5.2
  '--+------+--'      Temp      53.3 C
     ||||||||
                      Uptime    0d 1h 23m 45s
                      Reset     Power-on
                      Tasks     13

                      Heap      172 / 323 KB (53% free)
                      MinFree   165 KB
                      MaxBlock  107 KB
                      Flash     4096 KB QIO 80 MHz
                      Sketch    1025 / 4097 KB

                      Mode      STA
                      WiFi      [███░] Hamza -64 dBm ch11
                      IP        192.168.1.101
                      Gateway   192.168.1.1
                      DNS       192.168.1.1
                      MAC       C0:49:EF:D0:3F:E0
                      Hostname  esp32-D03FE0
                      TxPower   19.5 dBm

                      Tunnel    self-hosted
                      URL       https://...
```

## Features

- **Full system diagnostics** — chip, cores, temp, heap, flash, SPIFFS, LittleFS, PSRAM, WiFi signal bars
- **Neofetch-inspired** — IC-chip ASCII art with side-by-side info
- **ESPLogger** — Python/FastAPI-style logging with levels (`logger.info()`, `logger.error()`, `logger.request()`)
- **Thread-safe** — FreeRTOS mutex on ESP32, no-op on ESP8266
- **Auto-detects tunnel** — shows [esp32-tunnel](https://github.com/HamzaYslmn/esp32-tunnel) status if linked
- **ESP32 + ESP8266** — works on all Espressif boards

## Installation

### Arduino Library Manager

1. **Sketch → Include Library → Manage Libraries**
2. Search **"espfetch"**
3. **Install**

### Manual

Download ZIP → **Sketch → Include Library → Add .ZIP Library**

## Quick Start

```cpp
#include <espfetch.h>
#include <WiFi.h>

void setup() {
  rtosSerial.begin(115200);
  WiFi.begin("SSID", "PASS");
  while (WiFi.status() != WL_CONNECTED) delay(500);

  logger.info("WiFi connected: %s", WiFi.localIP().toString().c_str());
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (espfetch.check(cmd)) return;
  }
}
```

Type `/espfetch` in Serial Monitor → see system info.

## ESPLogger

Python-style logging with 5 levels:

```cpp
logger.debug("x = %d", x);          // DEBUG:    x = 42
logger.info("Server started");       // INFO:     Server started
logger.warning("Low memory: %d", h); // WARNING:  Low memory: 1024
logger.error("Connection failed");   // ERROR:    Connection failed
logger.critical("System halt");      // CRITICAL: System halt

logger.request("1.2.3.4", "GET", "/", 200);
// INFO:     1.2.3.4 - "GET /" 200

logger.setLevel(ESPLogger::LVL_WARNING);  // hide DEBUG + INFO
```

Thread-safe on ESP32 via FreeRTOS mutex. Default level: `LVL_INFO`.

## API

### espfetch

| Function | Description |
|---|---|
| `espfetch.check(cmd)` | Returns `true` if cmd was handled (`/espfetch`, `/fetch`, `/info`) |
| `espfetch.print()` | Print system info (use `print(true)` for full details) |
| `espfetch.printJSON()` | Print system info as JSON |

### ESPLogger

| Function | Description |
|---|---|
| `logger.debug(fmt, ...)` | Debug level message |
| `logger.info(fmt, ...)` | Info level message |
| `logger.warning(fmt, ...)` | Warning level message |
| `logger.error(fmt, ...)` | Error level message |
| `logger.critical(fmt, ...)` | Critical level message |
| `logger.request(ip, method, path, status)` | FastAPI-style request log |
| `logger.setLevel(level)` | Set minimum level (`LVL_DEBUG` .. `LVL_CRITICAL`) |

## Displayed Info

| Section | Fields |
|---|---|
| **Chip** | Model, revision, cores, frequency, SDK, internal temperature |
| **Runtime** | Uptime, reset reason, FreeRTOS task count |
| **Memory** | Heap (free/total/%), min free, max block, flash, sketch, PSRAM, SPIFFS, LittleFS |
| **Network** | WiFi signal bars, SSID, RSSI, channel, IP, gateway, DNS, MAC, hostname, TX power |
| **Tunnel** | Provider, status, URL (auto-detected from esp32-tunnel) |

## JSON Output

`espfetch json` (or `espfetch.printJSON()`) outputs a single JSON object:

| Field | Type | Platform | Description |
|---|---|---|---|
| `chip` | string | both | Chip model and revision |
| `cores` | int | both | Number of CPU cores |
| `freq_mhz` | int | both | CPU frequency in MHz |
| `sdk` | string | both | SDK version |
| `temp_c` | float | ESP32 | Internal temperature (°C) |
| `chip_id` | string | ESP32 | eFuse MAC as hex |
| `fw_md5` | string | ESP32 | Sketch MD5 hash |
| `uptime_s` | int | both | Uptime in seconds |
| `reset` | string | both | Last reset reason |
| `boot_count` | int | ESP32 | Total boot count (persisted in NVS) |
| `tasks` | int | ESP32 | FreeRTOS task count |
| `heap_free` | int | both | Free heap bytes |
| `heap_total` | int | ESP32 | Total heap bytes |
| `heap_min_free` | int | ESP32 | Minimum free heap ever |
| `heap_max_block` | int | ESP32 | Largest allocatable block |
| `flash_kb` | int | both | Flash size in KB |
| `sketch_kb` | int | both | Sketch size in KB |
| `sketch_total_kb` | int | both | Total sketch space in KB |
| `psram_free` | int | ESP32* | Free PSRAM (*if present) |
| `psram_total` | int | ESP32* | Total PSRAM (*if present) |
| `wifi_mode` | string | both | STA / AP / AP+STA / OFF |
| `ssid` | string | both* | Connected SSID (*if connected) |
| `rssi` | int | both* | Signal strength in dBm |
| `channel` | int | both* | WiFi channel |
| `ip` | string | both* | Local IP address |
| `mac` | string | both | MAC address |

## Integration with esp32-tunnel

espfetch auto-detects [esp32-tunnel](https://github.com/HamzaYslmn/esp32-tunnel) via C++ weak symbols. If esp32-tunnel is linked, tunnel info (provider, URL, last IP) appears automatically — no configuration needed.

To integrate your own library the same way, define these functions:

```cpp
String tunnelURL();
const char* tunnelProviderName();
bool tunnelReady();
String tunnelLastIP();
```

espfetch declares them as `__attribute__((weak))` stubs that return empty/false by default. Your strong definitions override them at link time.

## Companion Libraries

- [esp32-tunnel](https://github.com/HamzaYslmn/esp32-tunnel) — expose your ESP32 to the internet
- [esp-rtosSerial](https://github.com/HamzaYslmn/esp-rtosSerial) — thread-safe Serial reads for FreeRTOS

## License

MIT

## Author

**Hamza Yesilmen** — [@HamzaYslmn](https://github.com/HamzaYslmn)
