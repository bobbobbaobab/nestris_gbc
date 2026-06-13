#include <stdint.h>

typedef struct SaveData {
    uint16_t magic;
    uint32_t high_score;
    uint16_t checksum;
    uint8_t selected_level;
    uint8_t music_on;
    uint8_t das_delay;
    uint8_t arr_delay;
    uint16_t settings_checksum;
} SaveData;

#pragma dataseg DATA_0
SaveData save_data;
