/*
 * esplogger.h — Python-style logger for ESP32/ESP8266
 *
 * Part of the espfetch library.
 * On ESP32: outputs via rtosSerial (thread-safe).
 * On ESP8266: outputs via Serial.
 *
 *   logger.info("Server started on port %d", port);
 *   logger.warning("Low memory: %d bytes", freeHeap);
 *   logger.request(ip, "GET", "/", 200);   // FastAPI-style
 *
 * Output:
 *   INFO:     Server started on port 80
 *   WARNING:  Low memory: 12340 bytes
 *   INFO:     203.0.113.42 - "GET /" 200
 */

#ifndef ESPLOGGER_H
#define ESPLOGGER_H

#include <Arduino.h>

class ESPLogger {
public:
  enum Level { LVL_DEBUG, LVL_INFO, LVL_WARNING, LVL_ERROR, LVL_CRITICAL };

  void setLevel(Level l) { _level = l; }
  Level getLevel() const { return _level; }

  void debug(const char* fmt, ...)    __attribute__((format(printf, 2, 3)));
  void info(const char* fmt, ...)     __attribute__((format(printf, 2, 3)));
  void warning(const char* fmt, ...)  __attribute__((format(printf, 2, 3)));
  void error(const char* fmt, ...)    __attribute__((format(printf, 2, 3)));
  void critical(const char* fmt, ...) __attribute__((format(printf, 2, 3)));

  // FastAPI-style request log: INFO:     ip - "METHOD /path" status
  void request(const char* ip, const char* method, const char* path, int status);

private:
  Level _level = LVL_INFO;
  void _log(Level l, const char* tag, const char* fmt, va_list ap);
};

extern ESPLogger logger;

#endif
