#include "ESPNowBridge.h"

#include <WiFi.h>
#include <esp_wifi.h>

#ifdef WITH_ESPNOW_BRIDGE

ESPNowBridge *ESPNowBridge::_instance = nullptr;


void ESPNowBridge::recv_cb(const uint8_t *mac, const uint8_t *data, int32_t len) {
  if (_instance) {
    Serial.printf("[ESP-NOW][RX-CB] from %02X:%02X:%02X:%02X:%02X:%02X len=%d\n",
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], len);
    _instance->onDataRecv(mac, data, len);
  }
}

void ESPNowBridge::send_cb(const uint8_t *mac, esp_now_send_status_t status) {
  if (_instance) {
    Serial.printf("[ESP-NOW][TX-CB] to %02X:%02X:%02X:%02X:%02X:%02X status=%d\n",
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], status);
    _instance->onDataSent(mac, status);
  }
}

ESPNowBridge::ESPNowBridge(NodePrefs *prefs, mesh::PacketManager *mgr, mesh::RTCClock *rtc)
    : BridgeBase(prefs, mgr, rtc), _rx_buffer_pos(0) {
  _instance = this;
}

void ESPNowBridge::begin() {
  Serial.println("[ESP-NOW] Initializing bridge...");

  Serial.println("[ESP-NOW] Setting WiFi STA mode");
  WiFi.mode(WIFI_STA);

  // --- VALIDAR CANAL ---
int channel = _prefs->bridge_channel;
if (channel < 1 || channel > 13) {
    Serial.printf("[ESP-NOW] Invalid channel %d, forcing channel 1\n", channel);
    channel = 1;
}

Serial.printf("[ESP-NOW] Setting channel to %d\n", channel);
esp_err_t ch = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
if (ch != ESP_OK) {
    Serial.printf("[ESP-NOW] ERROR setting channel: %d\n", ch);
    return;
}

  if (ch != ESP_OK) {
    Serial.printf("[ESP-NOW] ERROR setting channel: %d\n", ch);
    return;
  }

  Serial.println("[ESP-NOW] Calling esp_now_init()");
  esp_err_t init = esp_now_init();
  if (init != ESP_OK) {
    Serial.printf("[ESP-NOW] ERROR esp_now_init(): %d\n", init);
    return;
  }

  Serial.println("[ESP-NOW] Registering callbacks");
  esp_now_register_recv_cb(recv_cb);
  esp_now_register_send_cb(send_cb);

  Serial.println("[ESP-NOW] Adding broadcast peer");
  esp_now_peer_info_t peerInfo = {};
  memset(&peerInfo, 0, sizeof(peerInfo));
  memset(peerInfo.peer_addr, 0xFF, ESP_NOW_ETH_ALEN);
  peerInfo.channel = _prefs->bridge_channel;
  peerInfo.encrypt = false;

  esp_err_t add = esp_now_add_peer(&peerInfo);
  if (add != ESP_OK) {
    Serial.printf("[ESP-NOW] ERROR adding broadcast peer: %d\n", add);
    return;
  }

  Serial.println("[ESP-NOW] Bridge initialized OK");
  _initialized = true;
}

void ESPNowBridge::end() {
  Serial.println("[ESP-NOW] Stopping bridge...");

  uint8_t broadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  esp_now_del_peer(broadcastAddress);

  esp_now_register_recv_cb(nullptr);
  esp_now_register_send_cb(nullptr);

  esp_now_deinit();

  WiFi.mode(WIFI_OFF);

  Serial.println("[ESP-NOW] Bridge stopped");
  _initialized = false;
}

void ESPNowBridge::loop() {
  // Apenas para debug
  static uint32_t last = 0;
  if (millis() - last > 5000) {
    last = millis();
    Serial.println("[ESP-NOW] Loop alive");
  }
}

void ESPNowBridge::xorCrypt(uint8_t *data, size_t len) {
  size_t keyLen = strlen(_prefs->bridge_secret);
  for (size_t i = 0; i < len; i++) {
    data[i] ^= _prefs->bridge_secret[i % keyLen];
  }
}

void ESPNowBridge::onDataRecv(const uint8_t *mac, const uint8_t *data, int32_t len) {
  Serial.printf("[ESP-NOW][RX] raw_len=%d\n", len);

  if (len < (BRIDGE_MAGIC_SIZE + BRIDGE_CHECKSUM_SIZE)) {
    Serial.printf("[ESP-NOW][RX] too small (%d)\n", len);
    return;
  }

  if (len > MAX_ESPNOW_PACKET_SIZE) {
    Serial.printf("[ESP-NOW][RX] too large (%d)\n", len);
    return;
  }

  uint16_t magic = (data[0] << 8) | data[1];
  if (magic != BRIDGE_PACKET_MAGIC) {
    Serial.printf("[ESP-NOW][RX] invalid magic 0x%04X\n", magic);
    return;
  }

  uint8_t decrypted[MAX_ESPNOW_PACKET_SIZE];
  size_t encryptedLen = len - BRIDGE_MAGIC_SIZE;
  memcpy(decrypted, data + BRIDGE_MAGIC_SIZE, encryptedLen);

  xorCrypt(decrypted, encryptedLen);

  uint16_t checksum = (decrypted[0] << 8) | decrypted[1];
  size_t payloadLen = encryptedLen - BRIDGE_CHECKSUM_SIZE;

  if (!validateChecksum(decrypted + BRIDGE_CHECKSUM_SIZE, payloadLen, checksum)) {
    Serial.printf("[ESP-NOW][RX] checksum mismatch (0x%04X)\n", checksum);
    return;
  }

  Serial.printf("[ESP-NOW][RX] payload_len=%d\n", payloadLen);

  mesh::Packet *pkt = _mgr->allocNew();
  if (!pkt) {
    Serial.println("[ESP-NOW][RX] allocNew() FAILED");
    return;
  }

  if (pkt->readFrom(decrypted + BRIDGE_CHECKSUM_SIZE, payloadLen)) {
    Serial.println("[ESP-NOW][RX] Packet injected into mesh");
    onPacketReceived(pkt);
  } else {
    Serial.println("[ESP-NOW][RX] Packet readFrom() FAILED");
    _mgr->free(pkt);
  }
}

void ESPNowBridge::onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.printf("[ESP-NOW][TX] status=%d\n", status);
}

void ESPNowBridge::sendPacket(mesh::Packet *packet) {
  if (!_initialized) {
    Serial.println("[ESP-NOW][TX] Bridge not initialized");
    return;
  }

  if (!packet) {
    Serial.println("[ESP-NOW][TX] NULL packet");
    return;
  }

  if (_seen_packets.hasSeen(packet)) {
    Serial.println("[ESP-NOW][TX] Duplicate packet ignored");
    return;
  }

  uint8_t sizingBuffer[MAX_PAYLOAD_SIZE];
  uint16_t meshPacketLen = packet->writeTo(sizingBuffer);

  if (meshPacketLen > MAX_PAYLOAD_SIZE) {
    Serial.printf("[ESP-NOW][TX] too large (%d)\n", meshPacketLen);
    return;
  }

  uint8_t buffer[MAX_ESPNOW_PACKET_SIZE];

  buffer[0] = (BRIDGE_PACKET_MAGIC >> 8) & 0xFF;
  buffer[1] = BRIDGE_PACKET_MAGIC & 0xFF;

  const size_t offset = BRIDGE_MAGIC_SIZE + BRIDGE_CHECKSUM_SIZE;
  memcpy(buffer + offset, sizingBuffer, meshPacketLen);

  uint16_t checksum = fletcher16(buffer + offset, meshPacketLen);
  buffer[2] = (checksum >> 8) & 0xFF;
  buffer[3] = checksum & 0xFF;

  xorCrypt(buffer + BRIDGE_MAGIC_SIZE, meshPacketLen + BRIDGE_CHECKSUM_SIZE);

  size_t total = BRIDGE_MAGIC_SIZE + BRIDGE_CHECKSUM_SIZE + meshPacketLen;

  uint8_t broadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  esp_err_t result = esp_now_send(broadcastAddress, buffer, total);

  Serial.printf("[ESP-NOW][TX] len=%d result=%d\n", meshPacketLen, result);
}

void ESPNowBridge::onPacketReceived(mesh::Packet *packet) {
  Serial.println("[ESP-NOW][MESH] Packet delivered to mesh");
  handleReceivedPacket(packet);
}

#endif
