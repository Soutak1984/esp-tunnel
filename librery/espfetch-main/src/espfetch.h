/*
 * espfetch.h — neofetch-style system info for ESP32/ESP8266
 *
 * Also includes ESPLogger (esplogger.h) for Python-style logging.
 *
 * Usage:
 *   #include <espfetch.h>
 *
 *   void setup() {
 *     logger.info("Started");
 *   }
 *
 *   void loop() {
 *     if (Serial.available()) {
 *       String cmd = Serial.readStringUntil('\n');
 *       cmd.trim();
 *       if (espfetch.check(cmd)) return;
 *     }
 *   }
 */

#ifndef ESPFETCH_H
#define ESPFETCH_H

#include <Arduino.h>
#include <rtosSerial.h>
#include "esplogger.h"

class ESPFetch {
public:
  bool check(const String &cmd);
  void print(bool full = false);
  void printJSON();
};

extern ESPFetch espfetch;

#endif
