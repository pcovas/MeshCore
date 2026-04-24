#include "CustomSX1262.h"
#include "helpers/esp32/PAControl.h"

CustomSX1262::CustomSX1262(Module* mod) : SX1262(mod) {}

#ifdef RP2040_PLATFORM
bool CustomSX1262::std_init(SPIClassRP2040* spi)
#else
bool CustomSX1262::std_init(SPIClass* spi)
#endif
{
#ifdef SX126X_DIO3_TCXO_VOLTAGE
    float tcxo = SX126X_DIO3_TCXO_VOLTAGE;
#else
    float tcxo = 1.6f;
#endif

#ifdef LORA_CR
    uint8_t cr = LORA_CR;
#else
    uint8_t cr = 5;
#endif

#if defined(P_LORA_SCLK)
    if (spi) spi->begin(P_LORA_SCLK, P_LORA_MISO, P_LORA_MOSI);
#endif

    int status = begin(LORA_FREQ, LORA_BW, LORA_SF, cr,
                       RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                       LORA_TX_POWER, 16, tcxo);

    if (status != RADIOLIB_ERR_NONE) {
        Serial.print("ERROR: radio init failed: ");
        Serial.println(status);
        return false;
    }

    setCRC(1);

#ifdef SX126X_CURRENT_LIMIT
    setCurrentLimit(SX126X_CURRENT_LIMIT);
#endif

#ifdef SX126X_DIO2_AS_RF_SWITCH
    setDio2AsRfSwitch(SX126X_DIO2_AS_RF_SWITCH);
#endif

#ifdef SX126X_RX_BOOSTED_GAIN
    setRxBoostedGainMode(SX126X_RX_BOOSTED_GAIN);
#endif

#if defined(SX126X_RXEN) || defined(SX126X_TXEN)
    #ifndef SX126X_RXEN
        #define SX126X_RXEN RADIOLIB_NC
    #endif
    #ifndef SX126X_TXEN
        #define SX126X_TXEN RADIOLIB_NC
    #endif
    setRfSwitchPins(SX126X_RXEN, SX126X_TXEN);
#endif

#ifdef SX126X_REGISTER_PATCH
    uint8_t r_data = 0;
    readRegister(0x8B5, &r_data, 1);
    r_data |= 0x01;
    writeRegister(0x8B5, &r_data, 1);
#endif

    // Ativar PA externo por defeito
    paOn();

    return true;
}

bool CustomSX1262::isReceiving() {
    uint16_t irq = getIrqFlags();
    return (irq & SX126X_IRQ_HEADER_VALID) ||
           (irq & SX126X_IRQ_PREAMBLE_DETECTED);
}
