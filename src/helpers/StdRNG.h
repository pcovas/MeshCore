#pragma once
#include <Mesh.h>

class StdRNG : public mesh::RNG {
public:
    StdRNG() {}

    void begin(uint32_t seed) {
        // Usa o gerador de números aleatórios do Arduino/ESP32
        randomSeed(seed);
    }

    void random(uint8_t* dest, size_t sz) override {
        for (size_t i = 0; i < sz; i++) {
            dest[i] = (uint8_t) ::random(0, 256);
        }
    }
};
