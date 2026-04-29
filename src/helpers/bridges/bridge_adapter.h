#pragma once
#include "NodePrefs.h"

struct BridgePrefs {
  uint8_t bridge_channel;
  const char* bridge_secret;
  uint16_t bridge_delay;
  // adiciona outros campos que o ESPNowBridge usa
};

BridgePrefs makeBridgePrefs(const NodePrefs* np);
