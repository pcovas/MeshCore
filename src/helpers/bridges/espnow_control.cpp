#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include "EspNowFlags.h"
#include "../../examples/companion_radio/MyMesh.h"
#include "../esp32/SerialWifiInterface.h"

extern MyMesh the_mesh;
extern SerialWifiInterface* wifi_iface;

bool espnow_active = true;

extern "C" {
    void espnowBridge_init(void* prefs, void* mgr, void* rtc);
    void espnowBridge_loop();
    void espnowBridge_end();
}

// ---------------------------------------------------------------------------
//  Funções auxiliares específicas para V4 (ESP32-S3)
// ---------------------------------------------------------------------------
#if defined(HELTEC_LORA_V4)
static void stopWifiForV4()
{
    Serial.println("[ESP-NOW] V4: stopping WiFi stack");

    // 1. Desligar STA/AP
    WiFi.disconnect(true, true);
    WiFi.softAPdisconnect(true);

    // 2. Parar driver WiFi
    esp_wifi_stop();
    esp_wifi_deinit();

    // 3. Garantir modo OFF
    WiFi.mode(WIFI_OFF);
    delay(50);

    Serial.println("[ESP-NOW] V4: WiFi fully OFF");
}

static void restoreWifiForV4()
{
    Serial.println("[ESP-NOW] V4: restoring WiFi");

    WiFi.mode(WIFI_STA);

    #ifdef WIFI_SSID
    WiFi.begin(WIFI_SSID, WIFI_PWD);
    #endif

    if (wifi_iface) {
        wifi_iface->startWifiServer(4403);
        Serial.println("[ESP-NOW] V4: WiFiServer restarted");
    }
}
#endif

// ---------------------------------------------------------------------------
//  ENABLE BRIDGE
// ---------------------------------------------------------------------------
extern "C" void enableEspNowBridge()
{
    Serial.println("[ESP-NOW] Request ENABLE");

#if defined(HELTEC_LORA_V4)
    stopWifiForV4();
#else
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
#endif

    NodePrefs* prefs = the_mesh.getNodePrefs();
    mesh::PacketManager* mgr = the_mesh.getPacketManager();
    mesh::RTCClock* rtc = the_mesh.getRTCClock();

    espnow_active = true;
    espnowBridge_init(prefs, mgr, rtc);

    Serial.println("[ESP-NOW] Bridge ENABLED");
}

// ---------------------------------------------------------------------------
//  DISABLE BRIDGE
// ---------------------------------------------------------------------------
extern "C" void disableEspNowBridge()
{
    Serial.println("[ESP-NOW] Request DISABLE");

    espnowBridge_end();
    espnow_active = false;

#if defined(HELTEC_LORA_V4)
    restoreWifiForV4();
#else
    Serial.println("[WiFi] STA remains OFF on V3");
#endif

    Serial.println("[ESP-NOW] Bridge DISABLED");
}

// ---------------------------------------------------------------------------
//  LOOP
// ---------------------------------------------------------------------------
extern "C" void espnowBridgeLoop()
{
    if (espnow_active) {
        espnowBridge_loop();
    }
}
