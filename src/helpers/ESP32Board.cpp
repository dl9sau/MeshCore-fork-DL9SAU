#ifdef ESP_PLATFORM

#include "ESP32Board.h"

// DL9SAU 2026-06-16: Compile-Gate erweitert. Vorher war OTA nur fuer
// Repeater/Room-Server (ADMIN_PASSWORD-Build). Companion-Radio darf
// ueber ENABLE_WIFI_OTA opt-in. DISABLE_WIFI_OTA ueberstimmt beides.
#if !defined(DISABLE_WIFI_OTA) && (defined(ADMIN_PASSWORD) || defined(ENABLE_WIFI_OTA))
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#include <SPIFFS.h>

// DL9SAU 2026-06-16: OTA-Session-State + 5-min Timeout.
// User-Wunsch 2026-06-16 'paranoid 5 min': nach 5 min ohne erfolgreichen
// Upload wird SoftAP automatisch geschlossen (via tickOTA aus loop()).
static AsyncWebServer* s_ota_server = NULL;
static unsigned long   s_ota_timeout_at = 0;
static bool            s_ota_active     = false;

static void ota_cleanup() {
  if (s_ota_server) {
    s_ota_server->end();
    delete s_ota_server;
    s_ota_server = NULL;
  }
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  s_ota_active = false;
  s_ota_timeout_at = 0;
}

bool ESP32Board::startOTAUpdate(const char* id, char reply[]) {
  if (s_ota_active) {
    sprintf(reply, "OTA already active: http://%s/update",
            WiFi.softAPIP().toString().c_str());
    return true;
  }
  inhibit_sleep = true;   // prevent sleep during OTA
  WiFi.softAP("MeshCore-OTA", NULL);

  sprintf(reply, "Started: http://%s/update (5 min timeout)",
          WiFi.softAPIP().toString().c_str());
  MESH_DEBUG_PRINTLN("startOTAUpdate: %s", reply);

  static char id_buf[60];
  sprintf(id_buf, "%s (%s)", id, getManufacturerName());
  static char home_buf[90];
  sprintf(home_buf, "<H2>Hi! I am a MeshCore node. ID: %s</H2>", id);

  s_ota_server = new AsyncWebServer(80);

  s_ota_server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", home_buf);
  });
  s_ota_server->on("/log", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/packet_log", "text/plain");
  });

  AsyncElegantOTA.setID(id_buf);
  AsyncElegantOTA.begin(s_ota_server);    // Start ElegantOTA
  s_ota_server->begin();

  s_ota_active = true;
  s_ota_timeout_at = millis() + 5UL * 60UL * 1000UL;  // 5 min
  return true;
}

bool ESP32Board::stopOTAUpdate(char reply[]) {
  if (!s_ota_active) {
    strcpy(reply, "OTA not active");
    return false;
  }
  ota_cleanup();
  inhibit_sleep = false;
  strcpy(reply, "OTA stopped");
  return true;
}

void ESP32Board::tickOTA() {
  if (!s_ota_active) return;
  // millis() rollover-safe Vergleich: (int32_t)(now - target) >= 0
  if ((int32_t)(millis() - s_ota_timeout_at) >= 0) {
    ota_cleanup();
    inhibit_sleep = false;
  }
}

#else
bool ESP32Board::startOTAUpdate(const char* id, char reply[]) {
  return false; // not supported
}
bool ESP32Board::stopOTAUpdate(char reply[]) {
  strcpy(reply, "Error");
  return false;
}
void ESP32Board::tickOTA() { /* no op */ }
#endif

#endif
