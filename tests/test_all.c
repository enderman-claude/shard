#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "shard/shard_open.h"
#include "shard/shard_load.h"
#include "shard/shard_link.h"
#include "elf/elf_open.h"
#include "elf/elf_load.h"

static int failures;
#define CHECK(cond, name) do { if (cond) { printf("ok   %s\n", name); } else { printf("FAIL %s\n", name); failures++; } } while (0)

static void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static void put32(uint8_t *p, uint32_t v) { for (int b = 0; b < 4; b++) p[b] = (uint8_t)(v >> (8 * b)); }
static void put64(uint8_t *p, uint64_t v) { for (int b = 0; b < 8; b++) p[b] = (uint8_t)(v >> (8 * b)); }

/* Layout: header 0..63, sections at 64 (3 x 40), relocs at 184 (1 x 24), bytes at 208.
 * code  vaddr 0,  4 bytes stored, flags R|X
 * data  vaddr 16, 8 bytes stored, flags R|W, reloc slot at 16
 * bss   vaddr 32, 16 bytes in memory, flags R|W
 * image_size 64. */
static size_t build_good(uint8_t *b)
{
    memset(b, 0, 256);
    memcpy(b, "SHRD", 4);
    put16(b + 4, 1);
    put16(b + 6, 62);
    put64(b + 12, 0);          /* entry = code start */
    put64(b + 20, 64);         /* image_size */
    put32(b + 28, 3);
    put32(b + 32, 64);
    put32(b + 36, 1);
    put32(b + 40, 184);

    uint8_t *s = b + 64;
    put32(s, 1); put32(s + 4, 5); put64(s + 8, 208); put64(s + 16, 0);  put64(s + 24, 4);  put64(s + 32, 4);
    s += 40;
    put32(s, 2); put32(s + 4, 3); put64(s + 8, 212); put64(s + 16, 16); put64(s + 24, 8); put64(s + 32, 8);
    s += 40;
    put32(s, 3); put32(s + 4, 3); put64(s + 8, 0);   put64(s + 16, 32); put64(s + 24, 0); put64(s + 32, 16);

    uint8_t *r = b + 184;
    put64(r, 16);
    put32(r + 8, 1);
    put64(r + 16, 0x10);

    b[208] = 0x90; b[209] = 0xC3; b[210] = 0xCC; b[211] = 0xCC;
    for (int i = 0; i < 8; i++) b[212 + i] = 0x11;
    return 220;
}

static void test_shard(void)
{
    uint8_t img[256];
    size_t n = build_good(img);
    struct shard_image si;
    CHECK(shard_open(img, n, &si) == SHARD_OK, "shard: good image opens");
    CHECK(si.entry == 0 && si.image_size == 64, "shard: header fields read");

    uint8_t dst[64];
    memset(dst, 0xAA, sizeof dst);
    uint64_t entry = 0;
    uint64_t base = 0x400000;
    CHECK(shard_load(&si, dst, sizeof dst, base, &entry) == SHARD_OK, "shard: load succeeds");
    CHECK(entry == base, "shard: entry is base + entry");
    CHECK(dst[0] == 0x90 && dst[1] == 0xC3, "shard: code copied");
    uint64_t slot = 0;
    for (int b = 7; b >= 0; b--) slot = (slot << 8) | dst[16 + b];
    CHECK(slot == base + 0x10, "shard: RELATIVE reloc patched with base");
    int bss_zero = 1;
    for (int i = 32; i < 48; i++) if (dst[i] != 0) bss_zero = 0;
    CHECK(bss_zero, "shard: bss is zero");
    CHECK(dst[4] == 0 && dst[5] == 0, "shard: gap after code is zeroed");

    uint8_t bad[256];
    memcpy(bad, img, n);
    put32(bad + 8, SHARD_FLAG_HAS_IMPORTS);
    CHECK(shard_open(bad, n, &si) == SHARD_ERR_UNSUPPORTED, "shard: v2 import flag rejected");
    uint32_t icount = 0;
    if (shard_open(img, n, &si) != SHARD_OK) { failures++; }
    CHECK(shard_import_count(&si, &icount) == SHARD_ERR_UNSUPPORTED, "shard: import count is a stub");
    CHECK(shard_link(&si, dst, sizeof dst, base, 0, 0) == SHARD_ERR_UNSUPPORTED, "shard: link is a stub");

    memcpy(bad, img, n);
    bad[0] = 'X';
    CHECK(shard_open(bad, n, &si) == SHARD_ERR_MAGIC, "shard: bad magic rejected");

    memcpy(bad, img, n);
    put16(bad + 6, 0x28);
    CHECK(shard_open(bad, n, &si) == SHARD_ERR_MACHINE, "shard: wrong machine rejected");

    CHECK(shard_open(img, 100, &si) == SHARD_ERR_RANGE, "shard: truncated tables rejected");

    memcpy(bad, img, n);
    put64(bad + 184, 0);
    CHECK(shard_open(bad, n, &si) == SHARD_ERR_RELOC, "shard: reloc into code rejected");

    memcpy(bad, img, n);
    put64(bad + 64 + 40 + 16, 0);  /* move data to vaddr 0, overlapping code */
    CHECK(shard_open(bad, n, &si) == SHARD_ERR_SECTION, "shard: overlapping sections rejected");

    memcpy(bad, img, n);
    put64(bad + 20, 64 * 0 + 1);   /* image_size smaller than data+bss */
    CHECK(shard_open(bad, n, &si) == SHARD_ERR_RANGE, "shard: bounds violation rejected");

    struct shard_image small;
    CHECK(shard_open(img, n, &small) == SHARD_OK && shard_load(&small, dst, 8, base, &entry) != SHARD_OK,
          "shard: small dest rejected");
}

static int read_file(const char *path, uint8_t *buf, size_t cap, size_t *out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    *out = fread(buf, 1, cap, f);
    fclose(f);
    return 1;
}

static void test_elf(const char *fixture)
{
    static uint8_t file[65536];
    size_t n = 0;
    if (!read_file(fixture, file, sizeof file, &n)) {
        printf("FAIL elf: cannot read fixture %s\n", fixture);
        failures++;
        return;
    }
    struct elf_image ei;
    CHECK(elf_open(file, n, &ei) == 0, "elf: ET_DYN x86_64 opens");

    static uint8_t dst[65536];
    uint64_t entry = 0, span = 0;
    uint64_t base = 0x700000;
    CHECK(elf_load(&ei, dst, sizeof dst, base, &entry, &span) == 0, "elf: load succeeds");

    int found = 0;
    for (uint64_t off = 0; off + 8 <= span; off += 8) {
        uint64_t v = 0;
        for (int b = 7; b >= 0; b--) v = (v << 8) | dst[off + b];
        if (v >= base && v - base < span && strncmp((const char *)dst + (v - base), "shard-elf-ok", 12) == 0) {
            found = 1;
            break;
        }
    }
    CHECK(found, "elf: reloc points at loaded string");

    uint8_t bad[256];
    memcpy(bad, file, 256);
    bad[0] = 0;
    CHECK(elf_open(bad, n, &ei) != 0, "elf: bad magic rejected");
    memcpy(bad, file, 256);
    bad[4] = 1;
    CHECK(elf_open(bad, n, &ei) != 0, "elf: 32-bit class rejected");
    CHECK(elf_open(file, 32, &ei) != 0, "elf: short file rejected");

    static uint8_t dep[65536];
    size_t dn = 0;
    if (read_file("build/needs_lib.so", dep, sizeof dep, &dn) && elf_open(dep, dn, &ei) == 0) {
        static uint8_t dd[65536];
        uint64_t e2 = 0, s2 = 0;
        CHECK(elf_load(&ei, dd, sizeof dd, 0x700000, &e2, &s2) != 0, "elf: DT_NEEDED library rejected");
    } else {
        printf("FAIL elf: needs_lib fixture missing\n");
        failures++;
    }
}

int main(int argc, char **argv)
{
    test_shard();
    if (argc > 1) {
        test_elf(argv[1]);
    } else {
        printf("FAIL elf: no fixture path given\n");
        failures++;
    }
    printf("%s (%d failures)\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
