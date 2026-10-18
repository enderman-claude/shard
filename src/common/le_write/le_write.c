#include "shard/le_write.h"

void le_write16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

void le_write32(uint8_t *p, uint32_t v)
{
    for (int b = 0; b < 4; b++) {
        p[b] = (uint8_t)(v >> (8 * b));
    }
}

void le_write64(uint8_t *p, uint64_t v)
{
    for (int b = 0; b < 8; b++) {
        p[b] = (uint8_t)(v >> (8 * b));
    }
}
