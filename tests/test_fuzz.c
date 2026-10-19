/* Deterministic mutation test for the readers and loaders. Corrupts valid SHARD and ELF
 * images at random positions and checks that:
 *   - open never reads outside the input (run under ASan),
 *   - load never writes outside the destination (guard bytes are checked),
 *   - accepted images always load without touching guard bytes.
 * Usage: test_fuzz <elf.so> <sample.shard> [iterations] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "shard/shard_open.h"
#include "shard/shard_load.h"
#include "elf/elf_open.h"
#include "elf/elf_load.h"

#define DST_BYTES 65536
#define GUARD 64

static uint64_t rng = 0x9E3779B97F4A7C15ull;
static uint32_t next(void)
{
    rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
    return (uint32_t)rng;
}

static uint8_t *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)n);
    *len = b ? fread(b, 1, (size_t)n, f) : 0;
    fclose(f);
    return b;
}

static int guards_ok(const uint8_t *buf, size_t n)
{
    for (size_t i = 0; i < GUARD; i++) {
        if (buf[i] != 0xA5 || buf[GUARD + n + i] != 0x5A) return 0;
    }
    return 1;
}

static void arm(uint8_t *guarded, size_t n)
{
    memset(guarded, 0xA5, GUARD);
    memset(guarded + GUARD + n, 0x5A, GUARD);
}

static void mutate(uint8_t *img, size_t n)
{
    uint32_t edits = 1 + next() % 8;
    for (uint32_t k = 0; k < edits; k++) {
        size_t pos = next() % n;
        switch (next() % 3) {
        case 0: img[pos] = (uint8_t)next(); break;
        case 1: img[pos] ^= (uint8_t)(1u << (next() % 8)); break;
        default: img[pos] = 0xFF; break;
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    long iters = argc > 3 ? atol(argv[3]) : 20000;
    size_t elf_n = 0, shard_n = 0;
    uint8_t *elf_src = read_file(argv[1], &elf_n);
    uint8_t *shard_src = read_file(argv[2], &shard_n);
    if (!elf_src || !shard_src) { printf("FAIL cannot read inputs\n"); return 1; }

    int failures = 0;
    long accepted_shard = 0, accepted_elf = 0;
    uint8_t *img = malloc(elf_n > shard_n ? elf_n : shard_n);
    uint8_t *guarded = malloc(DST_BYTES + 2 * GUARD);

    for (long it = 0; it < iters; it++) {
        int use_elf = it & 1;
        size_t n = use_elf ? elf_n : shard_n;
        memcpy(img, use_elf ? elf_src : shard_src, n);
        mutate(img, n);
        arm(guarded, DST_BYTES);
        uint8_t *dst = guarded + GUARD;

        if (use_elf) {
            struct elf_image ei;
            if (elf_open(img, n, &ei) == 0) {
                uint64_t entry = 0, span = 0;
                elf_load(&ei, dst, DST_BYTES, 0x400000, &entry, &span);
                accepted_elf += 1;
            }
        } else {
            struct shard_image si;
            if (shard_open(img, n, &si) == SHARD_OK) {
                uint64_t entry = 0;
                shard_load(&si, dst, DST_BYTES, 0x400000, &entry);
                accepted_shard += 1;
            }
        }
        if (!guards_ok(guarded, DST_BYTES)) {
            printf("FAIL guard bytes overwritten at iteration %ld (%s)\n", it, use_elf ? "elf" : "shard");
            failures++;
            if (failures > 5) break;
        }
    }
    free(img);
    free(guarded);
    free(elf_src);
    free(shard_src);
    printf("iterations %ld: accepted shard %ld, elf %ld, failures %d\n", iters, accepted_shard, accepted_elf, failures);
    printf("%s\n", failures ? "FAILED" : "PASSED");
    return failures ? 1 : 0;
}
