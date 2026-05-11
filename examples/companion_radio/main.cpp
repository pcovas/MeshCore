#include <Arduino.h>
#include "MyMesh.h"
#include "target.h"
#include "helpers/esp32/MultiInterface.h"
#include "helpers/esp32/SerialBLEInterface.h"
#include "helpers/esp32/SerialWifiInterface.h"

#include "NodePrefs.h"

DataStore store(SPIFFS, rtc_clock);

#if defined(HELTEC_LORA_V3)
extern HeltecV3Board board;
#elif defined(HELTEC_LORA_V4)
extern HeltecV4Board board;
#else
#error "Nenhuma board definida! Define HELTEC_LORA_V3 ou HELTEC_LORA_V4"
#endif

extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;
extern SimpleMeshTables tables;

SerialBLEInterface ble_if;
SerialWifiInterface wifi_if;
MultiInterface serial_interface;

#ifdef DISPLAY_CLASS
#include "ui-new/UITask.h"
UITask ui_task(&board, &serial_interface);
#endif


StdRNG fast_rng;

MyMesh the_mesh(
    radio_driver,
    fast_rng,
    rtc_clock,
    tables,
    store,
    &ui_task
);


void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("BOOT OK");
    
    SPIFFS.begin(false);
    //SPIFFS.begin(true); apenas se quiser formatar caso falhe, mas cuidado que apaga tudo!
    Serial.println("SPIFFS OK");

    board.begin();
    Serial.println("BOARD OK");

    // Inicializar rádio SX1262 (OBRIGATÓRIO)
    if (!radio_init()) {
        Serial.println("SX1262 init FAILED");
    } else {
        Serial.println("SX1262 init OK");
    }

    // ---- BLE CORRETO ----
    char dev_name[32];
    snprintf(dev_name, sizeof(dev_name), "MeshCore-%06X", (uint32_t)ESP.getEfuseMac());
    Serial.println(dev_name);

    ble_if.begin("MeshCore-", dev_name, 123456);
    Serial.println("BLE OK");

    // ---- SEM WIFI ----
    serial_interface.setBLE(&ble_if);
    serial_interface.setWiFi(nullptr);
    Serial.println("INTERFACE OK");

    // ---- UI ----
#ifdef DISPLAY_CLASS
    display.begin();
    ui_task.begin(&display, &sensors, the_mesh.getNodePrefs());
    Serial.println("UI OK");
#endif

    // ---- MESH (APENAS UMA VEZ) ----
    the_mesh.begin(true);
    Serial.println("MESH BEGIN OK");

    the_mesh.startInterface(serial_interface);
    Serial.println("MESH START OK");
}

void loop() {
  the_mesh.loop();
//sensors.loop();
  #ifdef DISPLAY_CLASS
  ui_task.loop();
#endif
//rtc_clock.tick();
}
