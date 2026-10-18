#ifndef CONVERT_SHARD_TO_ELF_H
#define CONVERT_SHARD_TO_ELF_H
#include "shard/shard_open.h"

/* SHARELF: converts a validated SHARD image into a plain ELF64 ET_DYN x86_64 file
 * that the ELF subset loader accepts. Layout: one zero-size anchor PT_LOAD at vaddr 0,
 * one PT_LOAD per SHARD section, one RW PT_LOAD for the dynamic table and RELA entries,
 * and a PT_DYNAMIC pointing at that table. Each RELATIVE record becomes an
 * R_X86_64_RELATIVE RELA entry. */

/* Writes the required output size in bytes. */
int shard_to_elf_size(const struct shard_image *img, size_t *size_out);

/* Writes the ELF into out (cap bytes). *written is the number of bytes produced. */
int shard_to_elf(const struct shard_image *img, uint8_t *out, size_t cap, size_t *written);

#endif
