/*
 * esplogger.cpp — Python-style logger for ESP32/ESP8266
 *
 * On ESP32: outputs via rtosSerial (thread-safe, mutex-protected).
 * On ESP8266: outputs via Serial (single-core, no mutex needed).
 */

#include "esplogger.h"

#include <rtosSerial.h>
#define _LOG_PRINTLN(s) rtosSerial.println(s)

ESPLogger logger;

static const char* _tags[] = {
  "DEBUG:    ", "INFO:     ", "WARNING:  ", "ERROR:    ", "CRITICAL: "
};

void ESPLogger::_log(Level l, const char* tag, const char* fmt, va_list ap) {
  if (l < _level) return;
  char buf[512];
  vsnprintf(buf, sizeof(buf), fmt, ap);
  char line[540];
  snprintf(line, sizeof(line), "%s%s", tag, buf);
  _LOG_PRINTLN(line);
}

void ESPLogger::debug(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  _log(LVL_DEBUG, _tags[0], fmt, ap);
  va_end(ap);
}

void ESPLogger::info(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  _log(LVL_INFO, _tags[1], fmt, ap);
  va_end(ap);
}

void ESPLogger::warning(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  _log(LVL_WARNING, _tags[2], fmt, ap);
  va_end(ap);
}

void ESPLogger::error(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  _log(LVL_ERROR, _tags[3], fmt, ap);
  va_end(ap);
}

void ESPLogger::critical(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  _log(LVL_CRITICAL, _tags[4], fmt, ap);
  va_end(ap);
}

void ESPLogger::request(const char* ip, const char* method, const char* path, int status) {
  if (LVL_INFO < _level) return;
  char line[256];
  snprintf(line, sizeof(line), "INFO:     %s - \"%s %s\" %d", ip, method, path, status);
  _LOG_PRINTLN(line);
}
