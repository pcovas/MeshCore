#pragma once
#include <stdint.h>

struct BridgePrefs {
    uint8_t bridge_channel;
    const char* bridge_secret;
    uint16_t bridge_delay;
};
