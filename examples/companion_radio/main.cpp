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
    
    SPIFFS.begin(true);
    // ⚠️ Apenas para este boot — reset de prefs
    //SPIFFS.remove("/prefs.bin");
    // depois disto, o MyMesh vai criar prefs novos
    
    the_mesh.begin(true);
    Serial.println("SPIFFS OK");

    board.begin();
    Serial.println("BOARD OK");

     // Inicializar rádio SX1262 (OBRIGATÓRIO)
    if (!radio_init()) {
        Serial.println("SX1262 init FAILED");
    } else {
        Serial.println("SX1262 init OK");
    }

    // carregar prefs
    NodePrefs* prefs = the_mesh.getNodePrefs();

    // aplicar LoRa ANTES do Mesh arrancar
    radio_set_params(prefs->freq, prefs->bw, prefs->sf, prefs->cr);
    radio_set_tx_power(prefs->tx_power_dbm);


    #ifdef DISPLAY_CLASS
    display.begin();                 // <- inicializa o SSD1306 / driver
    ui_task.begin(&display, &sensors, the_mesh.getNodePrefs());
    Serial.println("UI OK");
    #endif



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

    the_mesh.startInterface(serial_interface);
    Serial.println("MESH START OK");
    
    the_mesh.begin(true);
    Serial.println("MESH BEGIN OK");
}

void loop() {
  the_mesh.loop();
#ifdef DISPLAY_CLASS
  ui_task.loop();
#endif
}
