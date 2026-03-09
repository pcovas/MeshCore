#include <Arduino.h>
#include <Mesh.h>
#include "MyMesh.h"

#include <WiFi.h>

// Interfaces
#include "helpers/esp32/SerialBLEInterface.h"
#include "helpers/esp32/SerialWifiInterface.h"
#include "helpers/esp32/MultiInterface.h"

#define DEBUG_SERIAL Serial

String g_wifi_ip = "";

SerialBLEInterface ble_if;
SerialWifiInterface wifi_if;
MultiInterface serial_interface;

#ifndef TCP_PORT
#define TCP_PORT 5000
#endif

// Believe it or not, this std C function is busted on some platforms!
static uint32_t _atoi(const char* sp) {
  uint32_t n = 0;
  while (*sp && *sp >= '0' && *sp <= '9') {
    n *= 10;
    n += (*sp++ - '0');
  }
  return n;
}

/* ---------------- FILESYSTEM ---------------- */
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  #include <InternalFileSystem.h>
  #if defined(QSPIFLASH)
    #include <CustomLFS_QSPIFlash.h>
    DataStore store(InternalFS, QSPIFlash, rtc_clock);
  #else
    #if defined(EXTRAFS)
      #include <CustomLFS.h>
      CustomLFS ExtraFS(0xD4000, 0x19000, 128);
      DataStore store(InternalFS, ExtraFS, rtc_clock);
    #else
      DataStore store(InternalFS, rtc_clock);
    #endif
  #endif

#elif defined(RP2040_PLATFORM)
  #include <LittleFS.h>
  DataStore store(LittleFS, rtc_clock);

#elif defined(ESP32)
  #include <SPIFFS.h>
  DataStore store(SPIFFS, rtc_clock);

#else
  #error "Unsupported platform"
#endif

/* ---------------- GLOBAL OBJECTS ---------------- */
#ifdef DISPLAY_CLASS
  #include "UITask.h"
  UITask ui_task(&board, &serial_interface);
#endif

StdRNG fast_rng;
SimpleMeshTables tables;

MyMesh the_mesh(
    radio_driver,
    fast_rng,
    rtc_clock,
    tables,
    store
#ifdef DISPLAY_CLASS
    , &ui_task
#endif
);

/* ---------------- HALT ---------------- */
void halt() {
  while (1);
}

/* ======================================================
 *                        SETUP
 * ====================================================== */
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("USB OK");

  DEBUG_SERIAL.begin(115200);
  delay(200);
  DEBUG_SERIAL.println("Debug ativo");

  board.begin();

  /* DISPLAY INIT */
  DisplayDriver* disp = NULL;
#ifdef DISPLAY_CLASS
  if (display.begin()) {
    disp = &display;
    disp->startFrame();
    disp->drawTextCentered(disp->width() / 2, 28, "Loading...");
    disp->endFrame();
  }
#endif

  /* RADIO INIT */
  if (!radio_init()) halt();
  fast_rng.begin(radio_get_rng_seed());

  /* FILESYSTEM INIT */
#if defined(ESP32)
  if (!SPIFFS.begin(false)) {
    Serial.println("[SPIFFS] Mount failed, formatting...");
    SPIFFS.begin(true);
  }
#endif
  store.begin();

  /* MESH INIT */
  the_mesh.begin(
#ifdef DISPLAY_CLASS
      disp != NULL
#else
      false
#endif
  );

  /* ======================================================
   *                BLE INIT
   * ====================================================== */
#ifdef BLE_PIN_CODE
  {
    char dev_name[48];
sprintf(dev_name, "%s", BLE_NAME_PREFIX);

ble_if.begin(dev_name, the_mesh.getNodePrefs()->node_name, the_mesh.getBLEPin());
serial_interface.setBLE(&ble_if);

  }
#endif

  /* ======================================================
   *                WIFI INIT (TCP)
   * ====================================================== */
#ifdef WIFI_SSID
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PWD);

  Serial.println("[WiFi] Connecting…");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(200);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    for (int i = 0; i < 10 && WiFi.localIP().toString() == "0.0.0.0"; i++) {
      delay(500);
      Serial.println("[WiFi] Waiting for DHCP…");
    }

    if (WiFi.localIP().toString() != "0.0.0.0") {
      g_wifi_ip = WiFi.localIP().toString();
      Serial.print("[WiFi] IP: ");
      Serial.println(g_wifi_ip);

      wifi_if.begin(TCP_PORT);
      serial_interface.setWiFi(&wifi_if);
    } else {
      Serial.println("[WiFi] DHCP failed");
      g_wifi_ip = "";
    }
  } else {
    Serial.println("[WiFi] Connection failed");
    g_wifi_ip = "";
  }
#endif

  /* ======================================================
   *                SELECT INITIAL TRANSPORT
   * ====================================================== */
  if (serial_interface.ble != nullptr) {
    serial_interface.setMode(MultiInterface::Mode::BLE);
  } else if (serial_interface.wifi != nullptr) {
    serial_interface.setMode(MultiInterface::Mode::WIFI);
  }

  the_mesh.startInterface(serial_interface);

  /* SENSORS */
  sensors.begin();

#ifdef DISPLAY_CLASS
  ui_task.begin(disp, &sensors, the_mesh.getNodePrefs());
#endif
}

/* ======================================================
 *                        LOOP
 * ====================================================== */
void loop() {
  the_mesh.loop();
  sensors.loop();

#ifdef DISPLAY_CLASS
  ui_task.loop();
#endif

  rtc_clock.tick();
}
