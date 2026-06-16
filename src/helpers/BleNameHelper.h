#pragma once

#include <stddef.h>
#include <string.h>

// DL9SAU 2026-06-16: BLE-Name-Sanitize Helper.
// ESP32 + NRF52 SerialBLEInterface haben identische Logik (Word-
// boundary-Truncate + ASCII-only). Wurde 2x parallel implementiert
// (ESP32 zuerst, NRF52 nachgezogen wegen 'T1000-E-BOOT'-Bug). Hier
// einmal zentral.
//
// Schritte:
//   1) Nur druckbares ASCII (0x20..0x7E) durchlassen -- UTF-8 und
//      Steuerzeichen lassen sd_ble_gap_device_name_set bzw.
//      esp_ble_gap_set_device_name scheitern.
//   2) Wenn laenger als max_len: am letzten Space <= max_len cutten
//      ('MeshCore-Foo Bar Baz' -> 'MeshCore-Foo Bar', nicht 'B').
//      Kein Space im Bereich? Hartes Truncate.
//   3) Trailing-Space-Trim.
//
// dst_buf: muss >= max_len+1 Byte sein (NUL).
// max_len: max sichtbare Zeichen (typisch 28).
// returns: Anzahl geschriebene Zeichen (ohne NUL).
inline size_t sanitizeBleName(const char* src, char* dst_buf,
                              size_t dst_size, size_t max_len) {
  if (dst_size == 0) return 0;
  // Step 1: ASCII-only Filter, ggf. truncate auf dst_size-1.
  size_t n = 0;
  for (size_t i = 0; src[i] && n < dst_size - 1; i++) {
    unsigned char c = (unsigned char)src[i];
    if (c >= 0x20 && c < 0x7F) dst_buf[n++] = (char)c;
  }
  dst_buf[n] = 0;
  // Step 2: Word-boundary-Truncate auf max_len.
  if (n > max_len) {
    int cut = -1;
    for (int i = (int)max_len - 1; i >= 0; i--) {
      if (dst_buf[i] == ' ') { cut = i; break; }
    }
    if (cut > 0) dst_buf[cut] = 0;
    else         dst_buf[max_len] = 0;
    n = strlen(dst_buf);
  }
  // Step 3: Trailing-Space-Trim.
  while (n > 0 && dst_buf[n - 1] == ' ') dst_buf[--n] = 0;
  return n;
}
