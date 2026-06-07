/*
 * espfetch.cpp — neofetch-style system info for ESP32 & ESP8266
 *
 * Buffers entire output into a String, then prints via rtosSerial
 * (thread-safe) in a single call for maximum throughput.
 */

#include "espfetch.h"
#include <WiFi.h>

#if defined(ESP32)
  #include <esp_system.h>
  #include <esp_partition.h>
  #include <esp_sleep.h>
  #include <esp_log.h>
  #include <SPIFFS.h>
  #include <LittleFS.h>
  #include <esp_ota_ops.h>
  #include <esp_app_desc.h>
  #include <nvs_flash.h>
  #include <esp_wifi.h>
  #include <esp_heap_caps.h>
  #if defined(CONFIG_SECURE_BOOT)
    #include <esp_secure_boot.h>
  #endif
  #if defined(CONFIG_FLASH_ENCRYPTION_ENABLED) || defined(CONFIG_SECURE_FLASH_ENC_ENABLED)
    #include <esp_flash_encrypt.h>
  #endif
  #include <rtosSerial.h>
  #define _FPRINT(x) rtosSerial.print(x)
#elif defined(ESP8266)
  #include <FS.h>
  #include <LittleFS.h>
  #include <rtosSerial.h>
  #define _FPRINT(x) rtosSerial.print(x)
#endif

// Weak stubs — overridden by esp32-tunnel if linked
__attribute__((weak)) String tunnelURL()   { return ""; }
__attribute__((weak)) bool   tunnelReady() { return false; }
__attribute__((weak)) const char* tunnelProviderName() { return ""; }
__attribute__((weak)) String tunnelLastIP() { return ""; }

ESPFetch espfetch;

// MARK: Boot count (NVS, ESP32 only)
#if defined(ESP32)
static uint32_t _bootCount() {
  static uint32_t count = 0;
  static bool done = false;
  if (done) return count;
  done = true;
  nvs_handle_t h;
  if (nvs_open("espfetch", NVS_READWRITE, &h) == ESP_OK) {
    nvs_get_u32(h, "boots", &count);
    count++;
    nvs_set_u32(h, "boots", count);
    nvs_commit(h);
    nvs_close(h);
  }
  return count;
}
#endif

// ── Box-drawing characters (UTF-8) ─────────────────────────

#define _TH "\xe2\x94\x80"  // ─ (thin horizontal)

// ── Kawaii logo (20 visual chars wide) ──────────────────────

#define _FACE "\xe2\x80\xa2\xe2\xa9\x8a\xe2\x80\xa2"  // •⩊•

#if defined(ESP32)
  #define _LOGO_NAME "       ESP-32       "
#elif defined(ESP8266)
  #define _LOGO_NAME "      ESP-8266      "
#else
  #define _LOGO_NAME "        ESP         "
#endif

static const char* _logo[] = {
  "                    ",
  "                    ",
  "        " _FACE "         ",
  _LOGO_NAME,
  "                    ",
  "                    ",
  "                    ",
};
#define _LOGO_N 7
#define _LOGO_W 20

// ── Output buffering ────────────────────────────────────────

static String _out;
static int _ln;
static const char _pad[] = "                    ";

static void _pr(const char* text = nullptr) {
  _out += (_ln < _LOGO_N) ? _logo[_ln] : _pad;
  _out += "  ";
  if (text) _out += text;
  _out += '\n';
  _ln++;
}

static void _sec(const char* title) {
  char b[96];
  int off = snprintf(b, sizeof(b), _TH _TH " %s ", title);
  int vis = 3 + strlen(title) + 1;
  while (vis++ < 32 && off + 3 < (int)sizeof(b)) {
    memcpy(b + off, "\xe2\x94\x80", 3);
    off += 3;
  }
  b[off] = '\0';
  _pr(b);
}

static void _flush() {
  _FPRINT(_out);
  _out = "";
}

// ── Helpers ─────────────────────────────────────────────────

static const char* _flashMode() {
  switch (ESP.getFlashChipMode()) {
    case FM_QIO:  return "QIO";
    case FM_QOUT: return "QOUT";
    case FM_DIO:  return "DIO";
    case FM_DOUT: return "DOUT";
    default:      return "?";
  }
}

static const char* _resetReason() {
#if defined(ESP32)
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "Power-on";
    case ESP_RST_SW:        return "Software";
    case ESP_RST_PANIC:     return "Panic";
    case ESP_RST_INT_WDT:   return "Int WDT";
    case ESP_RST_TASK_WDT:  return "Task WDT";
    case ESP_RST_WDT:       return "Watchdog";
    case ESP_RST_BROWNOUT:  return "Brownout";
    case ESP_RST_DEEPSLEEP: return "Deep Sleep";
    default:                return "Unknown";
  }
#elif defined(ESP8266)
  return ESP.getResetReason().c_str();
#endif
}

static const char* _wifiMode() {
  switch (WiFi.getMode()) {
    case WIFI_STA:    return "STA";
    case WIFI_AP:     return "AP";
    case WIFI_AP_STA: return "AP+STA";
    default:          return "OFF";
  }
}

static void _signalBars(char* out, int rssi) {
  // █ = \xE2\x96\x88  ░ = \xE2\x96\x91  (UTF-8, 3 bytes each)
  int bars = (rssi >= -50) ? 4 :
             (rssi >= -65) ? 3 :
             (rssi >= -75) ? 2 :
             (rssi >= -85) ? 1 : 0;
  char* p = out;
  for (int i = 0; i < 4; i++) {
    if (i < bars) { *p++ = '\xE2'; *p++ = '\x96'; *p++ = '\x88'; }   // █
    else          { *p++ = '\xE2'; *p++ = '\x96'; *p++ = '\x91'; }   // ░
  }
  *p = '\0';
}

// ── Print (neofetch style) ──────────────────────────────────

void ESPFetch::print(bool full) {
  _out = "";
  _out.reserve(full ? 8192 : 2048);
  _ln = 0;
  char b[128];
  _out += '\n';

  // Header
#if defined(ESP32)
  snprintf(b, sizeof(b), "espfetch@%s", WiFi.getHostname());
#elif defined(ESP8266)
  snprintf(b, sizeof(b), "espfetch@%s", WiFi.hostname().c_str());
#endif
  _pr(b);

  {
    char sep[96];
    int off = 0;
    for (int i = 0; i < 28 && off + 3 < (int)sizeof(sep); i++) {
      memcpy(sep + off, "\xe2\x94\x80", 3);
      off += 3;
    }
    sep[off] = '\0';
    _pr(sep);
  }

  // Hardware
#if defined(ESP32)
  snprintf(b, sizeof(b), "%-10s%s rev%d", "Chip", ESP.getChipModel(), ESP.getChipRevision());
  _pr(b);
  snprintf(b, sizeof(b), "%-10s%d @ %d MHz", "Cores", ESP.getChipCores(), ESP.getCpuFreqMHz());
#elif defined(ESP8266)
  snprintf(b, sizeof(b), "%-10sESP8266 (ID: %06X)", "Chip", ESP.getChipId());
  _pr(b);
  snprintf(b, sizeof(b), "%-10s1 @ %d MHz", "Cores", ESP.getCpuFreqMHz());
#endif
  _pr(b);
  snprintf(b, sizeof(b), "%-10s%s", "SDK", ESP.getSdkVersion());
  _pr(b);

#if defined(ESP32)
  if (full) {
    uint64_t mac = ESP.getEfuseMac();
    snprintf(b, sizeof(b), "%-10s%04X%08X", "ChipID",
      (uint16_t)(mac >> 32), (uint32_t)mac);
    _pr(b);
    snprintf(b, sizeof(b), "%-10s%s", "FW MD5", ESP.getSketchMD5().c_str());
    _pr(b);

    // MARK: Build info
    snprintf(b, sizeof(b), "%-10s%s %s", "Built", __DATE__, __TIME__);
    _pr(b);
#ifdef ESP_ARDUINO_VERSION_MAJOR
    snprintf(b, sizeof(b), "%-10s%d.%d.%d", "Arduino", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
    _pr(b);
#endif
  }

  float temp = temperatureRead();
  if (temp > -20 && temp < 120) {
    snprintf(b, sizeof(b), "%-10s%.1f C", "Temp", temp);
    _pr(b);
  }

#endif

  _pr();

  // System
  if (full) _sec("System");

  unsigned long sec = millis() / 1000;
  snprintf(b, sizeof(b), "%-10s%lud %luh %lum %lus", "Uptime",
    sec / 86400, (sec % 86400) / 3600, (sec % 3600) / 60, sec % 60);
  _pr(b);
  snprintf(b, sizeof(b), "%-10s%s", "Reset", _resetReason());
  _pr(b);

#if defined(ESP32)
  snprintf(b, sizeof(b), "%-10s%lu", "Boots", (unsigned long)_bootCount());
  _pr(b);

  if (full) {
    esp_sleep_wakeup_cause_t wake = esp_sleep_get_wakeup_cause();
    if (wake != ESP_SLEEP_WAKEUP_UNDEFINED) {
      const char* wr;
      switch (wake) {
        case ESP_SLEEP_WAKEUP_EXT0:      wr = "EXT0"; break;
        case ESP_SLEEP_WAKEUP_EXT1:      wr = "EXT1"; break;
        case ESP_SLEEP_WAKEUP_TIMER:     wr = "Timer"; break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:  wr = "Touch"; break;
        case ESP_SLEEP_WAKEUP_ULP:       wr = "ULP"; break;
        case ESP_SLEEP_WAKEUP_GPIO:      wr = "GPIO"; break;
        case ESP_SLEEP_WAKEUP_UART:      wr = "UART"; break;
        default:                         wr = "Other"; break;
      }
      snprintf(b, sizeof(b), "%-10s%s", "Wakeup", wr);
      _pr(b);
    }

    // MARK: Security
    bool secureBoot = false;
    bool flashEncrypt = false;
#if defined(CONFIG_SECURE_BOOT)
    secureBoot = esp_secure_boot_enabled();
#endif
#if defined(CONFIG_FLASH_ENCRYPTION_ENABLED) || defined(CONFIG_SECURE_FLASH_ENC_ENABLED)
    flashEncrypt = esp_flash_encryption_enabled();
#endif
    snprintf(b, sizeof(b), "%-10s%s", "SecBoot", secureBoot ? "Enabled" : "Disabled");
    _pr(b);
    snprintf(b, sizeof(b), "%-10s%s", "FlashEnc", flashEncrypt ? "Enabled" : "Disabled");
    _pr(b);
  }

  UBaseType_t taskCount = uxTaskGetNumberOfTasks();
  snprintf(b, sizeof(b), "%-10s%d", "Tasks", (int)taskCount);
  _pr(b);

  if (full) {
    TaskStatus_t *taskArr = (TaskStatus_t*)malloc(taskCount * sizeof(TaskStatus_t));
    if (taskArr) {
      UBaseType_t got = uxTaskGetSystemState(taskArr, taskCount, NULL);
      for (UBaseType_t i = 0; i < got; i++) {
        int coreId = (int)taskArr[i].xCoreID;
        snprintf(b, sizeof(b), "  %-12s stk:%4lu  core:%d  pri:%d",
          taskArr[i].pcTaskName,
          (unsigned long)taskArr[i].usStackHighWaterMark,
          coreId == 0x7FFFFFFF ? -1 : coreId,
          (int)taskArr[i].uxCurrentPriority);
        _pr(b);
      }
      free(taskArr);
    }
  }
#endif

  _pr();

  // Memory
  if (full) _sec("Memory");

#if defined(ESP32)
  uint32_t hT = ESP.getHeapSize(), hF = ESP.getFreeHeap();
  snprintf(b, sizeof(b), "%-10s%lu / %lu KB (%d%% free)", "Heap",
    (unsigned long)(hF / 1024), (unsigned long)(hT / 1024),
    hT ? (int)(hF * 100 / hT) : 0);
  _pr(b);

  if (hT > 0 && hF > 0) {
    uint32_t maxBlk = ESP.getMaxAllocHeap();
    int frag = 100 - (int)(maxBlk * 100 / hF);
    snprintf(b, sizeof(b), "%-10s%d%%", "HeapFrag", frag);
    _pr(b);
  }

  if (full) {
    snprintf(b, sizeof(b), "%-10s%lu KB", "MinFree", (unsigned long)(ESP.getMinFreeHeap() / 1024));
    _pr(b);
    snprintf(b, sizeof(b), "%-10s%lu KB", "MaxBlock", (unsigned long)(ESP.getMaxAllocHeap() / 1024));
    _pr(b);

    // MARK: DMA heap
    uint32_t dmaFree = heap_caps_get_free_size(MALLOC_CAP_DMA);
    uint32_t dmaTotal = heap_caps_get_total_size(MALLOC_CAP_DMA);
    if (dmaTotal > 0) {
      snprintf(b, sizeof(b), "%-10s%lu / %lu KB", "DMA Heap",
        (unsigned long)(dmaFree / 1024), (unsigned long)(dmaTotal / 1024));
      _pr(b);
    }
  }
#elif defined(ESP8266)
  uint32_t hF = ESP.getFreeHeap();
  snprintf(b, sizeof(b), "%-10s%lu KB free", "Heap", (unsigned long)(hF / 1024));
  _pr(b);
  if (full) {
    snprintf(b, sizeof(b), "%-10s%lu KB", "MaxBlock", (unsigned long)(ESP.getMaxFreeBlockSize() / 1024));
    _pr(b);
  }
#endif

  snprintf(b, sizeof(b), "%-10s%lu KB %s %lu MHz", "Flash",
    (unsigned long)(ESP.getFlashChipSize() / 1024), _flashMode(),
    (unsigned long)(ESP.getFlashChipSpeed() / 1000000));
  _pr(b);

  uint32_t skUsed = ESP.getSketchSize();
  uint32_t skFree = ESP.getFreeSketchSpace();
  snprintf(b, sizeof(b), "%-10s%lu / %lu KB (%lu KB free)", "Sketch",
    (unsigned long)(skUsed / 1024),
    (unsigned long)((skUsed + skFree) / 1024),
    (unsigned long)(skFree / 1024));
  _pr(b);

#if defined(ESP32)
  if (full) {
    uint32_t ps = ESP.getPsramSize();
    if (ps) {
      snprintf(b, sizeof(b), "%-10s%lu / %lu KB free", "PSRAM",
        (unsigned long)(ESP.getFreePsram() / 1024), (unsigned long)(ps / 1024));
      _pr(b);
    }

    const esp_partition_t *sp = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
    if (sp) {
      // MARK: Try SPIFFS first, then LittleFS (same partition subtype 0x82)
      esp_log_level_set("SPIFFS", ESP_LOG_NONE);
      bool spiffsOk = SPIFFS.begin(false);
      esp_log_level_set("SPIFFS", ESP_LOG_ERROR);
      if (spiffsOk && SPIFFS.totalBytes() > 0) {
        snprintf(b, sizeof(b), "%-10s%lu / %lu KB used (%lu KB max)", "SPIFFS",
          (unsigned long)(SPIFFS.usedBytes() / 1024),
          (unsigned long)(SPIFFS.totalBytes() / 1024),
          (unsigned long)(sp->size / 1024));
        _pr(b);
      } else {
        esp_log_level_set("esp_littlefs", ESP_LOG_NONE);
        bool lfsOk = LittleFS.begin(false);
        esp_log_level_set("esp_littlefs", ESP_LOG_ERROR);
        if (lfsOk) {
          snprintf(b, sizeof(b), "%-10s%lu / %lu KB used (%lu KB max)", "LittleFS",
            (unsigned long)(LittleFS.usedBytes() / 1024),
            (unsigned long)(LittleFS.totalBytes() / 1024),
            (unsigned long)(sp->size / 1024));
          _pr(b);
        } else {
          snprintf(b, sizeof(b), "%-10s%lu KB (not formatted)", "FS",
            (unsigned long)(sp->size / 1024));
          _pr(b);
        }
      }
    }

    // MARK: NVS stats
    nvs_stats_t nvsStat;
    if (nvs_get_stats("nvs", &nvsStat) == ESP_OK) {
      snprintf(b, sizeof(b), "%-10s%lu / %lu entries", "NVS",
        (unsigned long)nvsStat.used_entries,
        (unsigned long)nvsStat.total_entries);
      _pr(b);
    }

    // MARK: OTA info
    const esp_partition_t *runPart = esp_ota_get_running_partition();
    if (runPart) {
      snprintf(b, sizeof(b), "%-10s%s (%luKB)", "RunPart",
        runPart->label, (unsigned long)(runPart->size / 1024));
      _pr(b);
    }
    const esp_app_desc_t *appDesc = esp_app_get_description();
    if (appDesc) {
      snprintf(b, sizeof(b), "%-10s%s", "AppVer", appDesc->version);
      _pr(b);
      snprintf(b, sizeof(b), "%-10s%s %s", "Compiled", appDesc->date, appDesc->time);
      _pr(b);
    }

    _pr();
    _sec("Partitions");
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while (it) {
      const esp_partition_t *p = esp_partition_get(it);
      snprintf(b, sizeof(b), "  %-12s %4lu KB  (%s)",
        p->label,
        (unsigned long)(p->size / 1024),
        p->type == ESP_PARTITION_TYPE_APP ? "app" : "data");
      _pr(b);
      it = esp_partition_next(it);
    }
    esp_partition_iterator_release(it);
  }
#elif defined(ESP8266)
  if (full) {
    LittleFS.begin();
    FSInfo fs;
    if (LittleFS.info(fs) && fs.totalBytes > 0) {
      snprintf(b, sizeof(b), "%-10s%lu / %lu KB used", "LittleFS",
        (unsigned long)(fs.usedBytes / 1024),
        (unsigned long)(fs.totalBytes / 1024));
      _pr(b);
    }
  }
#endif

  _pr();

  // Network
  if (full) _sec("Network");

  if (WiFi.status() == WL_CONNECTED) {
    char sig[16];
    _signalBars(sig, WiFi.RSSI());
    snprintf(b, sizeof(b), "%-10s[%s] %s %d dBm ch%d", "WiFi",
      sig, WiFi.SSID().c_str(), WiFi.RSSI(), WiFi.channel());
    _pr(b);
    snprintf(b, sizeof(b), "%-10s%s", "IP", WiFi.localIP().toString().c_str());
    _pr(b);

    if (full) {
      snprintf(b, sizeof(b), "%-10s%s", "Gateway", WiFi.gatewayIP().toString().c_str());
      _pr(b);
      snprintf(b, sizeof(b), "%-10s%s", "DNS", WiFi.dnsIP().toString().c_str());
      _pr(b);
      snprintf(b, sizeof(b), "%-10s%s", "BSSID", WiFi.BSSIDstr().c_str());
      _pr(b);
    }
  } else {
    _pr("WiFi      disconnected");
  }

  if (full) {
    snprintf(b, sizeof(b), "%-10s%s", "MAC", WiFi.macAddress().c_str());
    _pr(b);

#if defined(ESP32)
    snprintf(b, sizeof(b), "%-10s%s", "Hostname", WiFi.getHostname());
    _pr(b);
    snprintf(b, sizeof(b), "%-10s%.1f dBm", "TxPower", WiFi.getTxPower() * 0.25);
    _pr(b);

    // MARK: WiFi advanced
    wifi_bandwidth_t bw;
    if (esp_wifi_get_bandwidth(WIFI_IF_STA, &bw) == ESP_OK) {
      snprintf(b, sizeof(b), "%-10s%s", "Band", bw == WIFI_BW_HT20 ? "20 MHz" : "40 MHz");
      _pr(b);
    }

    uint8_t proto = 0;
    if (esp_wifi_get_protocol(WIFI_IF_STA, &proto) == ESP_OK) {
      String modes;
      if (proto & WIFI_PROTOCOL_11B) modes += "b";
      if (proto & WIFI_PROTOCOL_11G) modes += "/g";
      if (proto & WIFI_PROTOCOL_11N) modes += "/n";
      snprintf(b, sizeof(b), "%-10s802.11%s", "Protocol", modes.c_str());
      _pr(b);
    }

    wifi_country_t country;
    if (esp_wifi_get_country(&country) == ESP_OK) {
      char cc[4] = {country.cc[0], country.cc[1], country.cc[2], '\0'};
      snprintf(b, sizeof(b), "%-10s%s (ch%d-%d)", "Country", cc,
        country.schan, country.schan + country.nchan - 1);
      _pr(b);
    }
#elif defined(ESP8266)
    snprintf(b, sizeof(b), "%-10s%s", "Hostname", WiFi.hostname().c_str());
    _pr(b);
#endif
  }

  // Tunnel (auto-detected via weak symbols)
  String turl = tunnelURL();
  if (turl.length()) {
    _pr();
    if (full) {
      _sec("Tunnel");
      const char *pn = tunnelProviderName();
      if (pn[0]) {
        snprintf(b, sizeof(b), "%-10s%s%s", "Provider", pn,
          tunnelReady() ? "" : " (connecting...)");
        _pr(b);
      }
    }
    snprintf(b, sizeof(b), "%-10s%s", "Tunnel", turl.c_str());
    _pr(b);
    if (full) {
      String lip = tunnelLastIP();
      if (lip.length()) {
        snprintf(b, sizeof(b), "%-10s%s", "LastIP", lip.c_str());
        _pr(b);
      }
    }
  }

  _out += '\n';
  _flush();
}

// ── Check ────────────────────────────────────────────────────

bool ESPFetch::check(const String &cmd) {
  String c = cmd;
  c.trim();
  c.toLowerCase();
  if (c.startsWith("/")) c = c.substring(1);

  if (c == "espfetch" || c == "fetch" || c == "info") {
    print(false);
    return true;
  }
  if (c == "espfetch --full" || c == "espfetch -v" || c == "espfetch full") {
    print(true);
    return true;
  }
  if (c == "espfetch json" || c == "espfetch-json") {
    printJSON();
    return true;
  }
  return false;
}

// ── JSON output ─────────────────────────────────────────────

void ESPFetch::printJSON() {
  String j;
  j.reserve(2048);
  char b[256];

  j += '{';

#if defined(ESP32)
  snprintf(b, sizeof(b), "\"chip\":\"%s rev%d\",\"cores\":%d",
    ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores());
#elif defined(ESP8266)
  snprintf(b, sizeof(b), "\"chip\":\"ESP8266 (ID: %06X)\",\"cores\":1", ESP.getChipId());
#endif
  j += b;

  snprintf(b, sizeof(b), ",\"freq_mhz\":%d,\"sdk\":\"%s\"",
    ESP.getCpuFreqMHz(), ESP.getSdkVersion());
  j += b;

#if defined(ESP32)
  float temp = temperatureRead();
  if (temp > -20 && temp < 120) {
    snprintf(b, sizeof(b), ",\"temp_c\":%.1f", temp);
    j += b;
  }
  uint64_t efuse = ESP.getEfuseMac();
  snprintf(b, sizeof(b), ",\"chip_id\":\"%04X%08X\",\"fw_md5\":\"%s\"",
    (uint16_t)(efuse >> 32), (uint32_t)efuse, ESP.getSketchMD5().c_str());
  j += b;
#endif

  snprintf(b, sizeof(b), ",\"uptime_s\":%lu,\"reset\":\"%s\"",
    millis() / 1000, _resetReason());
  j += b;

#if defined(ESP32)
  snprintf(b, sizeof(b), ",\"boot_count\":%lu,\"tasks\":%d",
    (unsigned long)_bootCount(), (int)uxTaskGetNumberOfTasks());
  j += b;

  snprintf(b, sizeof(b), ",\"heap_free\":%lu,\"heap_total\":%lu,\"heap_min_free\":%lu,\"heap_max_block\":%lu",
    (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getHeapSize(),
    (unsigned long)ESP.getMinFreeHeap(), (unsigned long)ESP.getMaxAllocHeap());
  j += b;
#elif defined(ESP8266)
  snprintf(b, sizeof(b), ",\"heap_free\":%lu", (unsigned long)ESP.getFreeHeap());
  j += b;
#endif

  snprintf(b, sizeof(b), ",\"flash_kb\":%lu,\"sketch_kb\":%lu,\"sketch_total_kb\":%lu",
    (unsigned long)(ESP.getFlashChipSize() / 1024),
    (unsigned long)(ESP.getSketchSize() / 1024),
    (unsigned long)((ESP.getSketchSize() + ESP.getFreeSketchSpace()) / 1024));
  j += b;

#if defined(ESP32)
  uint32_t psj = ESP.getPsramSize();
  if (psj) {
    snprintf(b, sizeof(b), ",\"psram_free\":%lu,\"psram_total\":%lu",
      (unsigned long)ESP.getFreePsram(), (unsigned long)psj);
    j += b;
  }
#endif

  snprintf(b, sizeof(b), ",\"wifi_mode\":\"%s\"", _wifiMode());
  j += b;

  if (WiFi.status() == WL_CONNECTED) {
    snprintf(b, sizeof(b), ",\"ssid\":\"%s\",\"rssi\":%d,\"channel\":%d,\"ip\":\"%s\"",
      WiFi.SSID().c_str(), WiFi.RSSI(), WiFi.channel(),
      WiFi.localIP().toString().c_str());
    j += b;
  }

  snprintf(b, sizeof(b), ",\"mac\":\"%s\"", WiFi.macAddress().c_str());
  j += b;

  j += "}\n";
  _FPRINT(j);
}
