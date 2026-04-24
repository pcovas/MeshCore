#pragma once

inline void paOn() {
#ifdef P_LORA_PA_POWER
    pinMode(P_LORA_PA_POWER, OUTPUT);
    digitalWrite(P_LORA_PA_POWER, HIGH);
#endif
#ifdef P_LORA_PA_EN
    pinMode(P_LORA_PA_EN, OUTPUT);
    digitalWrite(P_LORA_PA_EN, HIGH);
#endif
#ifdef P_LORA_PA_TX_EN
    pinMode(P_LORA_PA_TX_EN, OUTPUT);
    digitalWrite(P_LORA_PA_TX_EN, HIGH);
#endif
}

inline void paOff() {
#ifdef P_LORA_PA_POWER
    pinMode(P_LORA_PA_POWER, OUTPUT);
    digitalWrite(P_LORA_PA_POWER, LOW);
#endif
#ifdef P_LORA_PA_EN
    pinMode(P_LORA_PA_EN, OUTPUT);
    digitalWrite(P_LORA_PA_EN, LOW);
#endif
#ifdef P_LORA_PA_TX_EN
    pinMode(P_LORA_PA_TX_EN, OUTPUT);
    digitalWrite(P_LORA_PA_TX_EN, LOW);
#endif
}
