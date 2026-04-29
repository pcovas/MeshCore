#include "bridge_adapter.h"
#include <cstring>

BridgePrefs makeBridgePrefs(const NodePrefs* np) {
  BridgePrefs bp;
  if (np) {
    bp.bridge_channel = (uint8_t) (np->tx_power_dbm >= 0 ? np->tx_power_dbm : 1); // exemplo: ajusta conforme necessário
    bp.bridge_secret = "default_secret"; // substitui por campo real se existir
    bp.bridge_delay = 0; // ajusta conforme necessário
  } else {
    bp.bridge_channel = 1;
    bp.bridge_secret = "default_secret";
    bp.bridge_delay = 0;
  }
  return bp;
}
