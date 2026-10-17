#ifndef WOW_SOURCE_SOUNDINTERFACE_SOUNDINTERFACE2EMOTES_H
#define WOW_SOURCE_SOUNDINTERFACE_SOUNDINTERFACE2EMOTES_H

#include <cstddef>
#include <cstdint>

struct TEXTEMOTELOOKUP {
    // Custom: was [22][2] (race IDs 0-21, m_emotesTextId at 0x00B0); widened to every uint8_t race ID
    static const uint32_t NUM_RACES = 256;

    /* 0000 */ int32_t m_soundIds[NUM_RACES][2];
    /* 0800 */ int32_t m_emotesTextId;

    TEXTEMOTELOOKUP();
};

#endif
