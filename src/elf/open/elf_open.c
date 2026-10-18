#include "elf/elf_open.h"
#include "shard/le_read.h"

#define ELF_ERR_TOO_SHORT -1
#define ELF_ERR_MAGIC -2
#define ELF_ERR_CLASS -3
#define ELF_ERR_TYPE -4
#define ELF_ERR_MACHINE -5
#define ELF_ERR_RANGE -6

#define ET_DYN 3
#define EM_X86_64 62

int elf_open(const uint8_t *data, size_t len, struct elf_image *img)
{
    if (len < 64) {
        return ELF_ERR_TOO_SHORT;
    }
    if (data[0] != 0x7F || data[1] != 'E' || data[2] != 'L' || data[3] != 'F') {
        return ELF_ERR_MAGIC;
    }
    if (data[4] != 2 || data[5] != 1) { /* 64-bit, little-endian */
        return ELF_ERR_CLASS;
    }
    if (le_read16(data + 16) != ET_DYN) {
        return ELF_ERR_TYPE;
    }
    if (le_read16(data + 18) != EM_X86_64) {
        return ELF_ERR_MACHINE;
    }
    img->data = data;
    img->len = len;
    img->entry = le_read64(data + 24);
    img->phoff = le_read64(data + 32);
    img->phnum = le_read16(data + 56);
    if (le_read16(data + 54) != 56) { /* e_phentsize */
        return ELF_ERR_RANGE;
    }
    if (img->phoff > len || (uint64_t)img->phnum * 56 > len - img->phoff) {
        return ELF_ERR_RANGE;
    }
    return 0;
}
