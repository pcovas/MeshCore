#include "BridgePrefs.h"
#include "NodePrefs.h"

BridgePrefs makeBridgePrefs(const NodePrefs* np) {
    BridgePrefs bp;
    bp.bridge_channel = 1;          // define tu
    bp.bridge_secret  = "secret";   // define tu
    bp.bridge_delay   = 0;          // define tu
    return bp;
}
