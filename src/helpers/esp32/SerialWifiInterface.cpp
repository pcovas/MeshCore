#include "SerialWifiInterface.h"
#include <WiFi.h>
#include "../bridges/EspNowFlags.h"

void SerialWifiInterface::begin(int port) {
  // WiFi STA desativado permanentemente
}

void SerialWifiInterface::startWifiServer(int port) {
    Serial.println("[WiFi] startWifiServer ignorado — WiFi STA desativado (OPÇÃO B)");
    return;
}

void SerialWifiInterface::stopWifi() {
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
}

void SerialWifiInterface::enable() { 
  if (_isEnabled) return;
  _isEnabled = true;
  clearBuffers();
}

void SerialWifiInterface::disable() {
  _isEnabled = false;
}

size_t SerialWifiInterface::writeFrame(const uint8_t src[], size_t len) {
  if (len > MAX_FRAME_SIZE) return 0;

  if (deviceConnected && len > 0) {
    if (send_queue_len >= FRAME_QUEUE_SIZE) return 0;

    send_queue[send_queue_len].len = len;
    memcpy(send_queue[send_queue_len].buf, src, len);
    send_queue_len++;

    return len;
  }
  return 0;
}

bool SerialWifiInterface::isWriteBusy() const {
  return false;
}

bool SerialWifiInterface::hasReceivedFrameHeader() {
  return received_frame_header.type != 0 && received_frame_header.length != 0;
}

void SerialWifiInterface::resetReceivedFrameHeader() {
  received_frame_header.type = 0;
  received_frame_header.length = 0;
}

size_t SerialWifiInterface::checkRecvFrame(uint8_t dest[]) {
  // WiFi STA desativado → nunca há clientes
  return 0;
}

bool SerialWifiInterface::isConnected() const {
  return false;
}
