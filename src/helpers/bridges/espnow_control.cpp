#include <Arduino.h>
#include "../../examples/companion_radio/MyMesh.h"

extern MyMesh the_mesh;

extern "C" {
    void espnowBridge_init(void* prefs, void* mgr, void* rtc);
    void espnowBridge_loop();
    void espnowBridge_end();
}

extern "C" void enableEspNowBridge()
{
    NodePrefs* prefs = the_mesh.getNodePrefs();
    mesh::PacketManager* mgr = the_mesh.getPacketManager();
    mesh::RTCClock* rtc = the_mesh.getRTCClock();

    espnowBridge_init(prefs, mgr, rtc);
    Serial.println("[ESP-NOW] Bridge ENABLED");
}

extern "C" void disableEspNowBridge()
{
    espnowBridge_end();
    Serial.println("[ESP-NOW] Bridge DISABLED");
}

extern "C" void espnowBridgeLoop()
{
    espnowBridge_loop();
}
