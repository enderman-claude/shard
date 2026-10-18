/* elfpack: plain ELF64 ET_DYN x86_64 -> SHARD v1. The inverse of shardelf.
 * Usage: elfpack <input.elf> <output.shard>
 *
 * Each PT_LOAD becomes one SHARD section. Each RELATIVE or symbol-defined
 * R_X86_64_64 relocation becomes a SHARD RELATIVE record. Output is validated
 * with shard_open before it is written. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "elf/elf_open.h"
#include "shard/shard_open.h"
#include "shard/le_read.h"
#include "shard/le_write.h"

#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PF_X 1
#define PF_W 2
#define PF_R 4
#define DT_NULL 0
#define DT_NEEDED 1
#define DT_SYMTAB 6
#define DT_RELA 7
#define DT_RELASZ 8
#define R_X86_64_64 1
#define R_X86_64_RELATIVE 8

struct seg {
    uint32_t type;
    uint32_t flags;
    uint64_t off;
    uint64_t vaddr;
    uint64_t filesz;
    uint64_t memsz;
};

static uint8_t *file;
static size_t file_len;

static void read_seg(const struct elf_image *img, uint16_t i, struct seg *s)
{
    const uint8_t *p = img->data + img->phoff + (uint64_t)i * 56;
    s->type = le_read32(p);
    s->flags = le_read32(p + 4);
    s->off = le_read64(p + 8);
    s->vaddr = le_read64(p + 16);
    s->filesz = le_read64(p + 32);
    s->memsz = le_read64(p + 40);
}

/* File offset of [vaddr, vaddr+size), which must lie inside one PT_LOAD's file bytes. */
static int vaddr_to_off(const struct seg *segs, uint16_t n, uint64_t va, uint64_t size, uint64_t *off)
{
    for (uint16_t i = 0; i < n; i++) {
        if (segs[i].type != PT_LOAD) continue;
        if (va >= segs[i].vaddr && size <= segs[i].filesz && va - segs[i].vaddr <= segs[i].filesz - size) {
            *off = segs[i].off + (va - segs[i].vaddr);
            return 1;
        }
    }
    return 0;
}

static int fail(const char *msg)
{
    fprintf(stderr, "elfpack: %s\n", msg);
    return 1;
}

static void put_u32(uint8_t *p, uint32_t v) { le_write32(p, v); }
static void put_u64(uint8_t *p, uint64_t v) { le_write64(p, v); }

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <input.elf> <output.shard>\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) return fail("cannot read input");
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return fail("empty input"); }
    file = malloc((size_t)n);
    file_len = fread(file, 1, (size_t)n, f);
    fclose(f);

    struct elf_image img;
    if (elf_open(file, file_len, &img) != 0) return fail("not a supported ELF (need ET_DYN x86_64)");
    if (img.phnum == 0 || img.phnum > 64) return fail("unsupported program header count");

    struct seg segs[64];
    for (uint16_t i = 0; i < img.phnum; i++) read_seg(&img, i, &segs[i]);

    /* Image span and base. */
    uint64_t lo = UINT64_MAX, hi = 0;
    uint32_t sec_count = 0;
    for (uint16_t i = 0; i < img.phnum; i++) {
        if (segs[i].type != PT_LOAD || segs[i].memsz == 0) continue;
        if (segs[i].vaddr < lo) lo = segs[i].vaddr;
        if (segs[i].vaddr + segs[i].memsz > hi) hi = segs[i].vaddr + segs[i].memsz;
        sec_count++;
    }
    if (sec_count == 0) return fail("no loadable segments");
    if (sec_count > SHARD_MAX_SECTIONS) return fail("too many segments for SHARD");
    uint64_t image_size = hi - lo;

    /* Dynamic table: reject shared library dependencies, find RELA and SYMTAB. */
    uint64_t rela_va = 0, relasz = 0, symtab_va = 0;
    for (uint16_t i = 0; i < img.phnum; i++) {
        if (segs[i].type != PT_DYNAMIC) continue;
        uint64_t doff;
        if (!vaddr_to_off(segs, img.phnum, segs[i].vaddr, segs[i].filesz, &doff)) return fail("dynamic table out of file");
        for (uint64_t k = 0; k + 16 <= segs[i].filesz; k += 16) {
            uint64_t tag = le_read64(file + doff + k);
            uint64_t val = le_read64(file + doff + k + 8);
            if (tag == DT_NULL) break;
            if (tag == DT_NEEDED) return fail("image depends on a shared library; SHARD cannot express that");
            if (tag == DT_RELA) rela_va = val;
            if (tag == DT_RELASZ) relasz = val;
            if (tag == DT_SYMTAB) symtab_va = val;
        }
    }
    if (relasz % 24 != 0) return fail("relocation table size is not a multiple of 24");
    uint32_t rel_count = (uint32_t)(relasz / 24);

    /* Collect relocations first so the output layout is known. */
    static struct shard_reloc rel[4096];
    if (rel_count > 4096) return fail("too many relocations");
    for (uint32_t r = 0; r < rel_count; r++) {
        uint64_t roff;
        if (!vaddr_to_off(segs, img.phnum, rela_va + (uint64_t)r * 24, 24, &roff)) return fail("relocation table out of file");
        uint64_t r_offset = le_read64(file + roff);
        uint64_t r_info = le_read64(file + roff + 8);
        uint64_t addend = le_read64(file + roff + 16);
        uint32_t type = (uint32_t)r_info;
        uint64_t sym = r_info >> 32;
        uint64_t value;
        if (type == R_X86_64_RELATIVE && sym == 0) {
            value = addend;
        } else if (type == R_X86_64_64 && sym != 0) {
            uint64_t soff;
            if (symtab_va == 0 || !vaddr_to_off(segs, img.phnum, symtab_va + sym * 24, 24, &soff))
                return fail("symbol table out of file");
            if (le_read16(file + soff + 6) == 0) return fail("undefined symbol; SHARD cannot express imports");
            value = le_read64(file + soff + 8) + addend;
        } else {
            return fail("unsupported relocation type");
        }
        rel[r].offset = r_offset - lo;
        rel[r].type = SHARD_REL_RELATIVE;
        rel[r].addend = (int64_t)(value - lo);
    }

    /* Layout: header, section table, reloc table, section bytes. */
    uint64_t sec_off = SHARD_HDR_SIZE;
    uint64_t rel_off = sec_off + (uint64_t)sec_count * SHARD_SEC_SIZE;
    uint64_t cursor = rel_off + (uint64_t)rel_count * SHARD_REL_SIZE;
    size_t out_cap = (size_t)cursor;
    for (uint16_t i = 0; i < img.phnum; i++) {
        if (segs[i].type == PT_LOAD && segs[i].memsz != 0) out_cap += (size_t)segs[i].filesz;
    }
    uint8_t *out = calloc(1, out_cap);
    if (!out) return fail("out of memory");

    memcpy(out, "SHRD", 4);
    le_write16(out + 4, SHARD_VERSION);
    le_write16(out + 6, SHARD_MACHINE_X86_64);
    put_u64(out + 12, img.entry - lo);
    put_u64(out + 20, image_size);
    put_u32(out + 28, sec_count);
    put_u32(out + 32, (uint32_t)sec_off);
    put_u32(out + 36, rel_count);
    put_u32(out + 40, (uint32_t)rel_off);

    uint32_t si = 0;
    uint64_t data_pos = cursor;
    for (uint16_t i = 0; i < img.phnum; i++) {
        struct seg *s = &segs[i];
        if (s->type != PT_LOAD || s->memsz == 0) continue;
        uint8_t *e = out + sec_off + (uint64_t)si * SHARD_SEC_SIZE;
        uint32_t flags = 0;
        if (s->flags & PF_R) flags |= SHARD_F_READ;
        if (s->flags & PF_W) flags |= SHARD_F_WRITE;
        if (s->flags & PF_X) flags |= SHARD_F_EXEC;
        uint32_t type = s->filesz == 0 ? SHARD_SEC_BSS
                      : (s->flags & PF_X) && !(s->flags & PF_W) ? SHARD_SEC_CODE
                      : SHARD_SEC_DATA;
        put_u32(e, type);
        put_u32(e + 4, flags);
        put_u64(e + 8, s->filesz == 0 ? 0 : data_pos);
        put_u64(e + 16, s->vaddr - lo);
        put_u64(e + 24, s->filesz);
        put_u64(e + 32, s->memsz);
        if (s->filesz != 0) {
            memcpy(out + data_pos, file + s->off, (size_t)s->filesz);
            data_pos += s->filesz;
        }
        si++;
    }

    for (uint32_t r = 0; r < rel_count; r++) {
        uint8_t *e = out + rel_off + (uint64_t)r * SHARD_REL_SIZE;
        put_u64(e, rel[r].offset);
        put_u32(e + 8, rel[r].type);
        put_u64(e + 16, (uint64_t)rel[r].addend);
    }

    /* Validate the result with the same reader the OS uses. */
    struct shard_image check;
    if (shard_open(out, (size_t)data_pos, &check) != SHARD_OK) {
        free(out);
        return fail("internal error: produced an invalid SHARD image");
    }

    FILE *o = fopen(argv[2], "wb");
    if (!o || fwrite(out, 1, (size_t)data_pos, o) != (size_t)data_pos) return fail("cannot write output");
    fclose(o);
    free(out);
    free(file);
    return 0;
}
