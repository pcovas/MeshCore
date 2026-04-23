#pragma once
#include <cstdint> // For uint8_t, uint32_t

#define TELEM_MODE_DENY            0
#define TELEM_MODE_ALLOW_FLAGS     1     // use contact.flags
#define TELEM_MODE_ALLOW_ALL       2

#define ADVERT_LOC_NONE       0
#define ADVERT_LOC_SHARE      1

struct NodePrefs {  // persisted to file
  float airtime_factor;
  char node_name[32];
  float freq;
  uint8_t sf;
  uint8_t cr;
  uint8_t multi_acks;
  uint8_t manual_add_contacts;
  float bw;
  int8_t tx_power_dbm;
  uint8_t telemetry_mode_base;
  uint8_t telemetry_mode_loc;
  uint8_t telemetry_mode_env;
  float rx_delay_base;
  uint32_t ble_pin;
  uint8_t  advert_loc_policy;
  uint8_t  buzzer_quiet;
  uint8_t  gps_enabled;
  uint32_t gps_interval;
  uint8_t autoadd_config;
  uint8_t client_repeat;
  uint8_t path_hash_mode;
  uint8_t autoadd_max_hops;

  // novos campos vindos do repeater
  uint8_t disable_fwd;   // 0 = forwarding ON, 1 = OFF
  uint8_t flood_max;     // max hops para FLOOD
  uint8_t loop_detect;   // modo de deteção de loops
};
