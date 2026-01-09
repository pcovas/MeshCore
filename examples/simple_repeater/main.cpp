#include <Arduino.h>
#include <Mesh.h>
#include "MyMesh.h"

#ifdef DISPLAY_CLASS
  #include "UITask.h"
  static UITask ui_task(display);
#endif

StdRNG fast_rng;
SimpleMeshTables tables;

MyMesh the_mesh(board, radio_driver, *new ArduinoMillis(), fast_rng, rtc_clock, tables);

void halt() {
  while (1);
}

void setup() {
  // --- USB CDC (ESSENCIAL no ESP32-S3) ---
  Serial.begin(115200);
  delay(100);

  if (Serial) 
    { Serial.println("[BOOT] Heltec V4 Repeater + RS232 Bridge"); 
    }

  // --- UART para o RS232Bridge ---
  Serial1.begin(115200, SERIAL_8N1, WITH_RS232_BRIDGE_RX, WITH_RS232_BRIDGE_TX);
  Serial.println("[BOOT] Serial1 inicializado");

  // --- Inicialização do board ---
  board.begin();

#ifdef DISPLAY_CLASS
  if (display.begin()) {
    display.startFrame();
    display.setCursor(0, 0);
    display.print("Please wait...");
    display.endFrame();
  }
#endif

  if (!radio_init()) {
    Serial.println("[ERRO] radio_init falhou");
    halt();
  }

  fast_rng.begin(radio_get_rng_seed());

  // --- Filesystem ---
  FILESYSTEM* fs;
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  InternalFS.begin();
  fs = &InternalFS;
  IdentityStore store(InternalFS, "");
#elif defined(ESP32)
  SPIFFS.begin(true);
  fs = &SPIFFS;
  IdentityStore store(SPIFFS, "/identity");
#elif defined(RP2040_PLATFORM)
  LittleFS.begin();
  fs = &LittleFS;
  IdentityStore store(LittleFS, "/identity");
  store.begin();
#else
  #error "need to define filesystem"
#endif

  if (!store.load("_main", the_mesh.self_id)) {
    Serial.println("[INFO] Generating new keypair");
    the_mesh.self_id = radio_new_identity();
    int count = 0;
    while (count < 10 && 
          (the_mesh.self_id.pub_key[0] == 0x00 || the_mesh.self_id.pub_key[0] == 0xFF)) {
      the_mesh.self_id = radio_new_identity();
      count++;
    }
    store.save("_main", the_mesh.self_id);
  }

  Serial.print("[ID] Repeater ID: ");
  mesh::Utils::printHex(Serial, the_mesh.self_id.pub_key, PUB_KEY_SIZE);
  Serial.println();

  sensors.begin();

  // --- Inicia o Mesh (inclui RS232Bridge.begin()) ---
  the_mesh.begin(fs);
  Serial.println("[BOOT] Mesh iniciado");
  the_mesh.debugBridgeState();


#ifdef DISPLAY_CLASS
  ui_task.begin(the_mesh.getNodePrefs(), FIRMWARE_BUILD_DATE, FIRMWARE_VERSION);
#endif

  // Envia advertisement inicial
  the_mesh.sendSelfAdvertisement(16000);

  Serial.println("[BOOT] Setup completo\n");
}

void loop() {
  // CLI DESATIVADO — evita bloquear o Serial
  while (Serial.available()) {
    Serial.read(); // limpa buffer
  }

  the_mesh.loop();
  sensors.loop();

#ifdef DISPLAY_CLASS
  ui_task.loop();
#endif

  rtc_clock.tick();
}
