#include "ESPNowBridge.h"

#include <WiFi.h>
#include <esp_wifi.h>

#ifdef WITH_ESPNOW_BRIDGE

ESPNowBridge *ESPNowBridge::_instance = nullptr;

// ---------------------------------------------------------
// Funções utilitárias adicionadas
// ---------------------------------------------------------

static void printMacLabel(const char* label) {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  Serial.printf("[%s] MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                label, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static bool wifiEspNowInit(int channel) {
  static bool wifi_started = false;
  static bool espnow_inited = false;

  WiFi.mode(WIFI_STA);

  if (!wifi_started) {
    if (esp_wifi_start() != ESP_OK) {
      Serial.println("[WIFI] esp_wifi_start() failed");
      return false;
    }
    wifi_started = true;
  }

  if (channel < 1 || channel > 13) channel = 1;
  if (esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    Serial.println("[WIFI] set_channel failed");
    return false;
  }

  uint8_t primary;
  wifi_second_chan_t second;
  esp_wifi_get_channel(&primary, &second);
  Serial.printf("[WIFI] Effective channel = %d\n", primary);

  printMacLabel("WIFI");

  if (!espnow_inited) {
    if (esp_now_init() != ESP_OK) {
      Serial.println("[ESP-NOW] init failed");
      return false;
    }
    espnow_inited = true;
  }

  return true;
}

// ---------------------------------------------------------
// Callbacks
// ---------------------------------------------------------

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

// ---------------------------------------------------------
// Construtor
// ---------------------------------------------------------

ESPNowBridge::ESPNowBridge(NodePrefs *prefs, mesh::PacketManager *mgr, mesh::RTCClock *rtc)
    : BridgeBase(prefs, mgr, rtc), _rx_buffer_pos(0) {
  _instance = this;
}

// ---------------------------------------------------------
// Inicialização revista
// ---------------------------------------------------------

void ESPNowBridge::begin() {
  Serial.println("[ESP-NOW] Initializing bridge...");

  esp_wifi_stop();
  WiFi.mode(WIFI_OFF);
  delay(20);

  WiFi.mode(WIFI_STA);

  int channel = _prefs->bridge_channel;
  if (channel < 1 || channel > 13) {
    Serial.printf("[ESP-NOW] Invalid channel %d, forcing 1\n", channel);
    channel = 1;
  }

  if (!wifiEspNowInit(channel)) {
    Serial.println("[ESP-NOW] wifiEspNowInit failed");
    return;
  }

  Serial.println("[ESP-NOW] Registering callbacks");
  esp_now_register_recv_cb(recv_cb);
  esp_now_register_send_cb(send_cb);

  Serial.println("[ESP-NOW] Adding broadcast peer");
  esp_now_peer_info_t peerInfo = {};
  memset(&peerInfo, 0, sizeof(peerInfo));
  memset(peerInfo.peer_addr, 0xFF, ESP_NOW_ETH_ALEN);

  peerInfo.channel = channel;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  if (!esp_now_is_peer_exist(peerInfo.peer_addr)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("[ESP-NOW] ERROR adding broadcast peer");
    }
  }

  printMacLabel("BRIDGE");

  Serial.println("[ESP-NOW] Bridge initialized OK");
  _initialized = true;
  uint8_t test_msg[] = "BRIDGE-TEST";
esp_err_t r = esp_now_send((uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF", test_msg, sizeof(test_msg));
Serial.printf("[TEST] BRIDGE TEST SEND result=%d\n", r);

}

// ---------------------------------------------------------
// Encerramento
// ---------------------------------------------------------

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

// ---------------------------------------------------------
// Loop
// ---------------------------------------------------------

void ESPNowBridge::loop() {
  static uint32_t last = 0;
  if (millis() - last > 5000) {
    last = millis();
    Serial.println("[ESP-NOW] Loop alive");
  }
}

// ---------------------------------------------------------
// XOR crypto
// ---------------------------------------------------------

void ESPNowBridge::xorCrypt(uint8_t *data, size_t len) {
  size_t keyLen = strlen(_prefs->bridge_secret);
  for (size_t i = 0; i < len; i++) {
    data[i] ^= _prefs->bridge_secret[i % keyLen];
  }
}

// ---------------------------------------------------------
// RX
// ---------------------------------------------------------

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

// ---------------------------------------------------------
// TX
// ---------------------------------------------------------

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

// ---------------------------------------------------------
// Entrega ao mesh
// ---------------------------------------------------------

void ESPNowBridge::onPacketReceived(mesh::Packet *packet) {
  Serial.println("[ESP-NOW][MESH] Packet delivered to mesh");
  handleReceivedPacket(packet);
}

#endif
