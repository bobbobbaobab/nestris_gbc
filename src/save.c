#include <stdint.h>

typedef struct SaveData {
    uint16_t magic;
    uint32_t high_score;
    uint16_t checksum;
} SaveData;

#pragma dataseg DATA_0
SaveData save_data;
