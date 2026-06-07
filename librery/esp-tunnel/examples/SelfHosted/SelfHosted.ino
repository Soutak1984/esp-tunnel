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

 example api call :- http://103.194.228.110:8000/nexintel-esp/api/data?key=Ox0493fyuj756h653d84duhe45

                     http://103.194.228.110:8000/nexintel-esp/api/gpio/write?pin=2&state=1&key=Ox0493fyuj756h653d84duhe45
 */

#include <ESPAsyncWebServer.h>
#include <esp32tunnel.h>
#include <esp32tunnel_testpage.h>
#include <espfetch.h>
#include <rtosSerial.h>

// ── Configuration ────────────────────────────────────────────
const char *WIFI_SSID = "Nexintel";
const char *WIFI_PASS = "nspl$1234";

// Replace with your relay server + unique device ID
//const char *TUNNEL_SERVER = "https://esp32-tunnel-waa0.onrender.com/nexintel-esp";
const char *TUNNEL_SERVER = "http://103.194.228.110:8000/nexintel-esp";;


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

void setup() {
  rtosSerial.begin(115200);

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

void loop() {
  // MARK: Drive the tunnel (required in loop or FreeRTOS task)
  tunnelLoop(true);

  // Print URL once ready
  static bool logged = false;
  if (!logged && tunnelReady()) {
    logger.info("Tunnel live: %s (%s)", tunnelURL().c_str(), tunnelProviderName());
    logged = true;
  }

  // MARK: Serial commands
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n'); cmd.trim();
    if (cmd.length() == 0) return;
    if (espfetch.check(cmd)) return;  // /espfetch — system info
    if (cmd == "url") logger.info("Tunnel: %s", tunnelURL().c_str());
    if (cmd == "ip")  logger.info("Last IP: %s", tunnelLastIP().c_str());
  }
}
