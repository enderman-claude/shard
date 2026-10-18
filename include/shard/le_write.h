#ifndef SHARD_LE_WRITE_H
#define SHARD_LE_WRITE_H
#include <stddef.h>
#include <stdint.h>

/* Little-endian writes with no alignment requirement. Caller checks bounds. */
void le_write16(uint8_t *p, uint16_t v);
void le_write32(uint8_t *p, uint32_t v);
void le_write64(uint8_t *p, uint64_t v);

#endif
