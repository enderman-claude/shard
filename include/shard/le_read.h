#ifndef SHARD_LE_READ_H
#define SHARD_LE_READ_H
#include <stddef.h>
#include <stdint.h>

/* Little-endian reads with no alignment requirement. Caller checks bounds. */
uint16_t le_read16(const uint8_t *p);
uint32_t le_read32(const uint8_t *p);
uint64_t le_read64(const uint8_t *p);

#endif
