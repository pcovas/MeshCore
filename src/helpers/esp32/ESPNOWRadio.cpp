#include "ESPNOWRadio.h"
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

// ---------------------------------------------------------
// Funções utilitárias (iguais às do Bridge)
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

  printMacLabel("RADIO");

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
// Estado interno
// ---------------------------------------------------------

static uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static esp_now_peer_info_t peerInfo;
static volatile bool is_send_complete = false;
static esp_err_t last_send_result;
static uint8_t rx_buf[256];
static uint8_t last_rx_len = 0;

// ---------------------------------------------------------
// Callbacks
// ---------------------------------------------------------

static void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  is_send_complete = true;
  ESPNOW_DEBUG_PRINTLN("Send Status: %d", (int)status);
}

static void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  ESPNOW_DEBUG_PRINTLN("Recv: len = %d", len);
  memcpy(rx_buf, data, len);
  last_rx_len = len;
}

// ---------------------------------------------------------
// Inicialização revista
// ---------------------------------------------------------

void ESPNOWRadio::init() {
  int channel = 1;  // podes ajustar ou sincronizar com o Bridge

  if (!wifiEspNowInit(channel)) {
    ESPNOW_DEBUG_PRINTLN("wifiEspNowInit failed");
    return;
  }

  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
  esp_wifi_set_max_tx_power(80);

  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = channel;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  is_send_complete = true;

  if (!esp_now_is_peer_exist(peerInfo.peer_addr)) {
    if (esp_now_add_peer(&peerInfo) == ESP_OK) {
      ESPNOW_DEBUG_PRINTLN("init success");
    } else {
      ESPNOW_DEBUG_PRINTLN("Failed to add peer");
    }
  } else {
    ESPNOW_DEBUG_PRINTLN("Peer already exists");
  }
}

// ---------------------------------------------------------
// API pública
// ---------------------------------------------------------

void ESPNOWRadio::setTxPower(uint8_t dbm) {
  esp_wifi_set_max_tx_power(dbm * 4);
}

uint32_t ESPNOWRadio::intID() {
  uint8_t mac[8];
  memset(mac, 0, sizeof(mac));
  esp_efuse_mac_get_default(mac);
  uint32_t n, m;
  memcpy(&n, &mac[0], 4);
  memcpy(&m, &mac[4], 4);
  return n + m;
}

bool ESPNOWRadio::startSendRaw(const uint8_t* bytes, int len) {
  is_send_complete = false;
  esp_err_t result = esp_now_send(broadcastAddress, bytes, len);
  if (result == ESP_OK) {
    n_sent++;
    ESPNOW_DEBUG_PRINTLN("Send success");
    return true;
  }
  last_send_result = result;
  is_send_complete = true;
  ESPNOW_DEBUG_PRINTLN("Send failed: %d", result);
  return false;
}

bool ESPNOWRadio::isSendComplete() {
  return is_send_complete;
}

void ESPNOWRadio::onSendFinished() {
  is_send_complete = true;
}

bool ESPNOWRadio::isInRecvMode() const {
  return is_send_complete;
}

float ESPNOWRadio::getLastRSSI() const { return 0; }
float ESPNOWRadio::getLastSNR() const { return 0; }

int ESPNOWRadio::recvRaw(uint8_t* bytes, int sz) {
  int len = last_rx_len;
  if (last_rx_len > 0) {
    memcpy(bytes, rx_buf, last_rx_len);
    last_rx_len = 0;
    n_recv++;
  }
  return len;
}

uint32_t ESPNOWRadio::getEstAirtimeFor(int len_bytes) {
  return 4;
}
