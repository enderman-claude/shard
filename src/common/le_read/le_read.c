#include "shard/le_read.h"

uint16_t le_read16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

uint32_t le_read32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint64_t le_read64(const uint8_t *p)
{
    return (uint64_t)le_read32(p) | ((uint64_t)le_read32(p + 4) << 32);
}
