/*
 * BasicCLI.ino — espfetch standalone example
 *
 * Type /espfetch (or /fetch, /info) in Serial Monitor.
 *
 * Works with esp32-tunnel too — just add:
 *   #include <esp32tunnel.h>
 * and espfetch auto-detects tunnel status.
 */

#include <espfetch.h>

#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif

#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASS "YOUR_PASSWORD"

void setup() {
  rtosSerial.begin(115200);
  rtosSerial.println("\n=== espfetch ===");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) delay(500);

  if (WiFi.status() == WL_CONNECTED)
    rtosSerial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
  else
    rtosSerial.println("WiFi not connected");

  rtosSerial.println("Type /espfetch for system info");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (espfetch.check(cmd)) return;
    // Handle your own commands here:
    // if (cmd == "/mycommand") { ... }
  }
}
