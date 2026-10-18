#ifndef SHARD_TYPES_H
#define SHARD_TYPES_H
#include <stddef.h>
#include <stdint.h>

#define SHARD_HDR_SIZE 64
#define SHARD_SEC_SIZE 40
#define SHARD_REL_SIZE 24
#define SHARD_VERSION 1
#define SHARD_MACHINE_X86_64 62
#define SHARD_MAX_SECTIONS 64

#define SHARD_SEC_CODE 1
#define SHARD_SEC_DATA 2
#define SHARD_SEC_BSS 3

#define SHARD_F_READ 1u
#define SHARD_F_WRITE 2u
#define SHARD_F_EXEC 4u

#define SHARD_REL_RELATIVE 1

/* Header flags. v1 accepts none. HAS_IMPORTS is reserved for v2. */
#define SHARD_FLAG_HAS_IMPORTS 1u

/* Status codes. 0 is success; everything else is an error. */
enum shard_status {
    SHARD_OK = 0,
    SHARD_ERR_TOO_SHORT = -1,
    SHARD_ERR_MAGIC = -2,
    SHARD_ERR_VERSION = -3,
    SHARD_ERR_MACHINE = -4,
    SHARD_ERR_RANGE = -5,
    SHARD_ERR_SECTION = -6,
    SHARD_ERR_RELOC = -7,
    SHARD_ERR_DEST = -8,
    SHARD_ERR_INDEX = -9,
    SHARD_ERR_UNSUPPORTED = -10,
};

#endif
