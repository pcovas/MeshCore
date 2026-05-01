#include <Arduino.h>
#include <WiFi.h>

#include "EspNowFlags.h"
#include "../../examples/companion_radio/MyMesh.h"
#include "../esp32/SerialWifiInterface.h"

extern MyMesh the_mesh;
extern SerialWifiInterface* wifi_iface;

bool espnow_active = false;

extern "C" {
    void espnowBridge_init(void* prefs, void* mgr, void* rtc);
    void espnowBridge_loop();
    void espnowBridge_end();
}

extern "C" void enableEspNowBridge()
{
    Serial.println("[ESP-NOW] Request ENABLE");

    // Desliga WiFi completamente
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);

    NodePrefs* prefs = the_mesh.getNodePrefs();
    mesh::PacketManager* mgr = the_mesh.getPacketManager();
    mesh::RTCClock* rtc = the_mesh.getRTCClock();

    espnow_active = true;
    espnowBridge_init(prefs, mgr, rtc);

    Serial.println("[ESP-NOW] Bridge ENABLED");
}

extern "C" void disableEspNowBridge()
{
    Serial.println("[ESP-NOW] Request DISABLE");

    espnowBridge_end();
    espnow_active = false;

    // WiFi STA está desativado permanentemente na OPÇÃO B
    Serial.println("[WiFi] STA desativado permanentemente — não será reativado");

    Serial.println("[ESP-NOW] Bridge DISABLED");
}

extern "C" void espnowBridgeLoop()
{
    espnowBridge_loop();
}
