#pragma once

#include <Arduino.h>
#include <Mesh.h>
#include <RTClib.h>
#include <target.h>

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  #include <InternalFileSystem.h>
#elif defined(RP2040_PLATFORM)
  #include <LittleFS.h>
#elif defined(ESP32)
  #include <SPIFFS.h>
#endif

#ifdef WITH_RS232_BRIDGE
  #include "helpers/bridges/RS232Bridge.h"
  #define WITH_BRIDGE
#endif

#ifdef WITH_ESPNOW_BRIDGE
  #include "helpers/bridges/ESPNowBridge.h"
  #define WITH_BRIDGE
#endif

#include <helpers/AdvertDataHelpers.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/ClientACL.h>
#include <helpers/CommonCLI.h>
#include <helpers/IdentityStore.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/StatsFormatHelper.h>
#include <helpers/TxtDataHelpers.h>
#include <helpers/RegionMap.h>
#include "RateLimiter.h"

// ---------------- Bridge debug macros -------------------------------------
#ifdef BRIDGE_DEBUG
  #define BRIDGE_DBG_PRINTLN(msg) do { if (Serial) { Serial.println(msg); } } while(0)
  #define BRIDGE_DBG_PRINTF(fmt, ...) do { if (Serial) { Serial.printf(fmt, ##__VA_ARGS__); } } while(0)
#else
  #define BRIDGE_DBG_PRINTLN(msg)
  #define BRIDGE_DBG_PRINTF(fmt, ...)
#endif
// -------------------------------------------------------------------------

struct RepeaterStats {
  uint16_t batt_milli_volts;
  uint16_t curr_tx_queue_len;
  int16_t  noise_floor;
  int16_t  last_rssi;
  uint32_t n_packets_recv;
  uint32_t n_packets_sent;
  uint32_t total_air_time_secs;
  uint32_t total_up_time_secs;
  uint32_t n_sent_flood, n_sent_direct;
  uint32_t n_recv_flood, n_recv_direct;
  uint16_t err_events;
  int16_t  last_snr;
  uint16_t n_direct_dups, n_flood_dups;
  uint32_t total_rx_air_time_secs;
};

#ifndef MAX_CLIENTS
  #define MAX_CLIENTS 32
#endif

struct NeighbourInfo {
  mesh::Identity id;
  uint32_t advert_timestamp;
  uint32_t heard_timestamp;
  int8_t snr;
};

#ifndef FIRMWARE_BUILD_DATE
  #define FIRMWARE_BUILD_DATE "30 Nov 2025"
#endif

#ifndef FIRMWARE_VERSION
  #define FIRMWARE_VERSION "v1.11.0"
#endif

#define FIRMWARE_ROLE "repeater"
#define PACKET_LOG_FILE "/packet_log"

class MyMesh : public mesh::Mesh, public CommonCLICallbacks {
  FILESYSTEM* _fs;
  uint32_t last_millis;
  uint64_t uptime_millis;
  unsigned long next_local_advert, next_flood_advert;
  bool _logging;
  NodePrefs _prefs;
  CommonCLI _cli;
  uint8_t reply_data[MAX_PACKET_PAYLOAD];
  ClientACL  acl;
  TransportKeyStore key_store;
  RegionMap region_map, temp_map;
  RegionEntry* load_stack[8];
  RegionEntry* recv_pkt_region;
  RateLimiter discover_limiter;
  bool region_load_active;
  unsigned long dirty_contacts_expiry;
#if MAX_NEIGHBOURS
  NeighbourInfo neighbours[MAX_NEIGHBOURS];
#endif
  CayenneLPP telemetry;
  unsigned long set_radio_at, revert_radio_at;
  float pending_freq;
  float pending_bw;
  uint8_t pending_sf;
  uint8_t pending_cr;
  int  matching_peer_indexes[MAX_CLIENTS];

#if defined(WITH_RS232_BRIDGE)
  RS232Bridge bridge;
#elif defined(WITH_ESPNOW_BRIDGE)
  ESPNowBridge bridge;
#endif

  void putNeighbour(const mesh::Identity& id, uint32_t timestamp, float snr);
  uint8_t handleLoginReq(const mesh::Identity& sender, const uint8_t* secret, uint32_t sender_timestamp, const uint8_t* data, bool is_flood);
  int handleRequest(ClientInfo* sender, uint32_t sender_timestamp, uint8_t* payload, size_t payload_len);
  mesh::Packet* createSelfAdvert();
  File openAppend(const char* fname);

protected:
  float getAirtimeBudgetFactor() const override { return _prefs.airtime_factor; }
  bool allowPacketForward(const mesh::Packet* packet) override;
  const char* getLogDateTime() override;
  void logRxRaw(float snr, float rssi, const uint8_t raw[], int len) override;
  void logRx(mesh::Packet* pkt, int len, float score) override;
  void logTx(mesh::Packet* pkt, int len) override;
  void logTxFail(mesh::Packet* pkt, int len) override;
  int calcRxDelay(float score, uint32_t air_time) const override;
  uint32_t getRetransmitDelay(const mesh::Packet* packet) override;
  uint32_t getDirectRetransmitDelay(const mesh::Packet* packet) override;
  int getInterferenceThreshold() const override { return _prefs.interference_threshold; }
  int getAGCResetInterval() const override { return ((int)_prefs.agc_reset_interval) * 4000; }
  uint8_t getExtraAckTransmitCount() const override { return _prefs.multi_acks; }

#if ENV_INCLUDE_GPS == 1
  void applyGpsPrefs() { sensors.setSettingValue("gps", _prefs.gps_enabled ? "1" : "0"); }
#endif

  bool filterRecvFloodPacket(mesh::Packet* pkt) override;
  void onAnonDataRecv(mesh::Packet* packet, const uint8_t* secret, const mesh::Identity& sender, uint8_t* data, size_t len) override;
  int searchPeersByHash(const uint8_t* hash) override;
  void getPeerSharedSecret(uint8_t* dest_secret, int peer_idx) override;
  void onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id, uint32_t timestamp, const uint8_t* app_data, size_t app_data_len);
  void onPeerDataRecv(mesh::Packet* packet, uint8_t type, int sender_idx, const uint8_t* secret, uint8_t* data, size_t len) override;
  bool onPeerPathRecv(mesh::Packet* packet, int sender_idx, const uint8_t* secret, uint8_t* path, uint8_t path_len, uint8_t extra_type, uint8_t* extra, uint8_t extra_len) override;
  void onControlDataRecv(mesh::Packet* packet) override;

public:
  MyMesh(mesh::MainBoard& board, mesh::Radio& radio, mesh::MillisecondClock& ms, mesh::RNG& rng, mesh::RTCClock& rtc, mesh::MeshTables& tables);
  void begin(FILESYSTEM* fs);

  const char* getFirmwareVer() override { return FIRMWARE_VERSION; }
  const char* getBuildDate() override { return FIRMWARE_BUILD_DATE; }
  const char* getRole() override { return FIRMWARE_ROLE; }
  const char* getNodeName() { return _prefs.node_name; }
  NodePrefs* getNodePrefs() { return &_prefs; }

  void savePrefs() override { _cli.savePrefs(_fs); }
  void applyTempRadioParams(float freq, float bw, uint8_t sf, uint8_t cr, int timeout_mins) override;
  bool formatFileSystem() override;
  void sendSelfAdvertisement(int delay_millis) override;
  void updateAdvertTimer() override;
  void updateFloodAdvertTimer() override;
  void setLoggingOn(bool enable) override { _logging = enable; }
  void eraseLogFile() override { _fs->remove(PACKET_LOG_FILE); }
  void dumpLogFile() override;
  void setTxPower(uint8_t power_dbm) override;
  void formatNeighborsReply(char *reply) override;
  void removeNeighbor(const uint8_t* pubkey, int key_len) override;
  void formatStatsReply(char *reply) override;
  void formatRadioStatsReply(char *reply) override;
  void formatPacketStatsReply(char *reply) override;
  mesh::LocalIdentity& getSelfId() override { return self_id; }
  void saveIdentity(const mesh::LocalIdentity& new_id) override;
  void clearStats() override;
  void handleCommand(uint32_t sender_timestamp, char* command, char* reply);
  void loop();

  // ---------- Bridge debug helpers (safe, compile-time guarded) ----------
  bool isBridgeCompiled() const {
  #if defined(WITH_BRIDGE)
    return true;
  #else
    return false;
  #endif
  }

  // Prints compile-time and prefs info about bridge. Call after Serial is ready.
  void debugBridgeState() {
  #ifdef BRIDGE_DEBUG
    if (!Serial) return;
    if (!isBridgeCompiled()) {
      Serial.println("[BRIDGE] Not compiled in (WITH_BRIDGE not defined)");
      return;
    }
    Serial.println("[BRIDGE] Bridge compiled into firmware.");
  #if defined(WITH_RS232_BRIDGE)
    Serial.println("[BRIDGE] Bridge type: RS232Bridge");
  #elif defined(WITH_ESPNOW_BRIDGE)
    Serial.println("[BRIDGE] Bridge type: ESPNowBridge");
  #else
    Serial.println("[BRIDGE] Bridge type: Unknown");
  #endif

    // Print prefs values (safe; prefs exist even if SPIFFS overrides later)
    Serial.print("[BRIDGE] prefs.bridge_enabled = ");
    Serial.println(_prefs.bridge_enabled ? "1" : "0");
    Serial.print("[BRIDGE] prefs.bridge_baud = ");
    Serial.println(_prefs.bridge_baud);
    Serial.print("[BRIDGE] prefs.bridge_channel = ");
    Serial.println(_prefs.bridge_channel);
    Serial.print("[BRIDGE] prefs.bridge_pkt_src = ");
    Serial.println(_prefs.bridge_pkt_src);
  #endif
  }
  // ----------------------------------------------------------------------

#if defined(WITH_BRIDGE)
  void setBridgeState(bool enable) override {
    if (enable == bridge.isRunning()) return;
    if (enable) {
      BRIDGE_DBG_PRINTLN("Calling bridge.begin()");
      bridge.begin();
    } else {
      BRIDGE_DBG_PRINTLN("Calling bridge.end()");
      bridge.end();
    }
  }

  void restartBridge() override {
    // guard: if bridge has no isRunning(), this will still compile if the bridge implements it
    if (!bridge.isRunning()) return;
    BRIDGE_DBG_PRINTLN("Restarting bridge");
    bridge.end();
    bridge.begin();
  }
#endif

}; // class MyMesh
