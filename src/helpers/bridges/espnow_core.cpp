#include "ESPNowBridge.h"

static ESPNowBridge* g_bridge = nullptr;

extern "C" void espnowBridge_init(void* prefs, void* mgr, void* rtc)
{
    if (!g_bridge) {
        g_bridge = new ESPNowBridge(
            (NodePrefs*)prefs,
            (mesh::PacketManager*)mgr,
            (mesh::RTCClock*)rtc
        );
        g_bridge->begin();
    }
}

extern "C" void espnowBridge_loop()
{
    if (g_bridge)
        g_bridge->loop();
}

extern "C" void espnowBridge_end()
{
    if (g_bridge) {
        g_bridge->end();
        delete g_bridge;
        g_bridge = nullptr;
    }
}
