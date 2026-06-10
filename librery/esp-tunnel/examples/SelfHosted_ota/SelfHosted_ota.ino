/*
 * SelfHosted.ino — Full-featured self-hosted relay tunnel
 *
 * Demonstrates ALL tunnel features with the self-hosted provider:
 *   - ESPAsyncWebServer proxy (default)
 *   - Global password protection
 *   - Per-route authentication (RouteConfig)
 *   - TLS CA cert verification
 *   - Handler mode (no proxy)
 *   - Serial commands (espfetch, url, ip)
 *
 * The relay server forwards browser HTTPS requests to this device over
 * a plain WebSocket — no TLS needed on the ESP, saving ~40 KB RAM.
 *
 * A free public relay is available at:
 *   https://esp32-tunnel-waa0.onrender.com
 *
 * Public URL:  https://esp32-tunnel-waa0.onrender.com/<your-device-id>
 *
 * Board: ESP32 or ESP8266

 file system:- Minimal SPIFFS (1.9MB APP / 190KB SPIFFS)

 example api call :- http://103.194.228.110:8000/nexintel-esp/api/data?key=Ox0493fyuj756h653d84duhe45

                     http://103.194.228.110:8000/nexintel-esp/api/gpio/write?pin=2&state=1&key=Ox0493fyuj756h653d84duhe45

                     http://103.194.228.110:8000/nexintel-esp/api/update?ver=v1.2.0&key=Ox0493fyuj756h653d84duhe45

                     http://103.194.228.110:8000/nexintel-esp/api/version?key=Ox0493fyuj756h653d84duhe45

                     
                     
 */

#include <ESPAsyncWebServer.h>
#include <esp32tunnel.h>
#include <esp32tunnel_testpage.h>
#include <espfetch.h>
#include <rtosSerial.h>

#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>

#define FW_VERSION "v1.0.0"

bool otaRequested = false;
String otaURL = "";

//String version="";
// ── Configuration ────────────────────────────────────────────
//const char *WIFI_SSID = "Nexintel";
//const char *WIFI_PASS = "nspl$1234";

const char *WIFI_SSID = "dlink-C6C1";
const char *WIFI_PASS = "9239200096";

// Replace with your relay server + unique device ID
//const char *TUNNEL_SERVER = "https://esp32-tunnel-waa0.onrender.com/nexintel-esp";
const char *TUNNEL_SERVER = "http://103.194.228.110:8000/nexintel-esp";

//const char* FW_URL ="https://raw.githubusercontent.com/youruser/firmware/main/firmware.bin";
//const char* FW_URL ="https://github.com/youruser/firmware/releases/download/" + version + "/firmware.bin" ;
// ─────────────────────────────────────────────────────────────

AsyncWebServer server(80);

// ── Authentication (uncomment ONE to use) ────────────────────
// Option A: Per-route passwords (longest prefix match wins)
// RouteConfig routes[] = {
//   {"/api",   "api-secret"},    // /api/*   → ?key=api-secret
//   {"/admin", "admin-pass"},    // /admin/* → ?key=admin-pass
//   {"/",      nullptr}          // everything else → public
// };
//
 //Option B: Global password (all routes)
 const char *GLOBAL_PASSWORD = "Ox0493fyuj756h653d84duhe45";
////////////////////////////////////////////////////////////////////////////////////////////////////
/*
void performOTA()
{
    WiFiClientSecure client;
    client.setInsecure();   // For testing

    Serial.println("Starting OTA...");

    t_httpUpdate_return ret =
        httpUpdate.update(client, FW_URL);

    switch(ret)
    {
        case HTTP_UPDATE_FAILED:

            Serial.printf(
                "Update failed (%d): %s\n",
                httpUpdate.getLastError(),
                httpUpdate.getLastErrorString().c_str()
            );
            break;

        case HTTP_UPDATE_NO_UPDATES:

            Serial.println("No updates available");
            break;

        case HTTP_UPDATE_OK:

            Serial.println("Update successful");
            break;
    }
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  rtosSerial.begin(115200);

Serial.println();
Serial.println("================================");
Serial.println("Firmware Version: " FW_VERSION);
Serial.println("================================");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  logger.info("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) delay(500);
  logger.info("WiFi: %s", WiFi.localIP().toString().c_str());

  // MARK: Local routes (served through the tunnel)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "text/html", TUN_TEST_HTML);
  });

  server.on("/ping", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "application/json",
      "{\"pong\":true,\"heap\":" + String(ESP.getFreeHeap()) +
      ",\"uptime\":" + String(millis() / 1000) + "}");
  });

  server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "application/json", "{\"sensor\":42,\"status\":\"ok\"}");
  });

  server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "text/html", "<h1>Admin Panel</h1><p>Protected area.</p>");
  });
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
server.on("/api/gpio/write", HTTP_GET, [](AsyncWebServerRequest *r) {

  if (!r->hasParam("pin") || !r->hasParam("state")) {
    r->send(400, "application/json",
            "{\"status\":\"error\",\"msg\":\"Missing pin/state\"}");
    return;
  }

  int pin = r->getParam("pin")->value().toInt();
  int state = r->getParam("state")->value().toInt();

  // Safety: only allow selected GPIOs
  switch(pin) {
    case 2:
    case 4:
    case 5:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 21:
    case 22:
    case 23:
      break;

    default:
      r->send(403, "application/json",
              "{\"status\":\"error\",\"msg\":\"GPIO not allowed\"}");
      return;
  }

  pinMode(pin, OUTPUT);
  digitalWrite(pin, state ? HIGH : LOW);

  String json = "{";
  json += "\"status\":\"ok\",";
  json += "\"pin\":" + String(pin) + ",";
  json += "\"state\":" + String(state);
  json += "}";

  r->send(200, "application/json", json);
});


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
server.on("/api/gpio/read", HTTP_GET, [](AsyncWebServerRequest *r) {

  if (!r->hasParam("pin")) {
    r->send(400, "application/json",
            "{\"status\":\"error\",\"msg\":\"Missing pin\"}");
    return;
  }

  int pin = r->getParam("pin")->value().toInt();

  pinMode(pin, INPUT_PULLUP);

  int value = digitalRead(pin);

  String json = "{";
  json += "\"pin\":" + String(pin) + ",";
  json += "\"value\":" + String(value);
  json += "}";

  r->send(200, "application/json", json);
});

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
server.on("/api/update", HTTP_GET,
[](AsyncWebServerRequest *r)
{
    if (!r->hasParam("ver"))
    {
        r->send(400, "application/json",
                "{\"error\":\"missing ver\"}");
        return;
    }

    String version = r->getParam("ver")->value();

    otaURL =
      "https://raw.githubusercontent.com/Soutak1984/esp-tunnel/main/firmware/" +
      version +
      "/firmware.bin";

    otaRequested = true;

    r->send(200,
            "application/json",
            "{\"status\":\"OTA scheduled\"}");
});
//.........................................................................................................................
server.on("/api/version", HTTP_GET,
[](AsyncWebServerRequest *r)
{
    r->send(200,
            "application/json",
            "{\"version\":\"" FW_VERSION "\"}");
});
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  server.begin();

  // MARK: Start tunnel — pick ONE:

  // No auth (public):
  //tunnelSetup(SELFHOST, TUNNEL_SERVER);

  // Option A — per-route auth: uncomment RouteConfig above
  // tunnelSetup(SELFHOST, TUNNEL_SERVER, routes);

  // Option B — global password: ?key=my-secret on all routes
   tunnelSetup(SELFHOST, TUNNEL_SERVER, GLOBAL_PASSWORD);

  // Optional: verify server TLS certificate
  // tunnelCACert(nullptr);  // nullptr = skip verification (insecure)
}

void loop()
{
    tunnelLoop(true);

    if (otaRequested)
    {
        otaRequested = false;

        Serial.println("Starting OTA");
        Serial.println(otaURL);

        WiFiClientSecure client;
        client.setInsecure();

        t_httpUpdate_return ret =
            httpUpdate.update(client, otaURL);

        switch(ret)
        {
            case HTTP_UPDATE_FAILED:
                Serial.printf("OTA Failed (%d): %s\n",
                    httpUpdate.getLastError(),
                    httpUpdate.getLastErrorString().c_str());
                break;

            case HTTP_UPDATE_NO_UPDATES:
                Serial.println("No updates");
                break;

            case HTTP_UPDATE_OK:
                Serial.println("Update OK - rebooting");
                break;
        }
    }
}
