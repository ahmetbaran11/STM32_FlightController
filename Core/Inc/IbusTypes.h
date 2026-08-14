#pragma once
#include <cstdint>

constexpr int IBUS_CHANNELS = 14;

struct RcInput{
    uint16_t channel[IBUS_CHANNELS];   // 1000-2000 us araligi
    bool linkOk;                        // zaman asimi olmadi mi
    uint32_t frameCount;				// teshis: alinan gecerli cerceve sayisi
};
