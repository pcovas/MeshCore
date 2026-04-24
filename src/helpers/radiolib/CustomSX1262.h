#pragma once

#include <RadioLib.h>

#define SX126X_IRQ_HEADER_VALID       0b0000010000
#define SX126X_IRQ_PREAMBLE_DETECTED  0x04

class CustomSX1262 : public SX1262 {
public:
    CustomSX1262(Module* mod);

#ifdef RP2040_PLATFORM
    bool std_init(SPIClassRP2040* spi = NULL);
#else
    bool std_init(SPIClass* spi = NULL);
#endif

    bool isReceiving();
};
