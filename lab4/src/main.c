/* main.c — SM3 correctness tests and performance benchmarks
 *
 * Tests all implementations against known test vectors, then benchmarks
 * throughput for various message sizes.
 */

#define _POSIX_C_SOURCE 199309L

#include "sm3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

/* ------------------------------------------------------------------ */
/*  Test vectors (from GM/T 0004-2012)                                */
/* ------------------------------------------------------------------ */

/* "abc" */
static const uint8_t tv1_msg[] = { 0x61, 0x62, 0x63 };
static const size_t  tv1_len = 3;
static const uint8_t tv1_hash[32] = {
    0x66, 0xC7, 0xF0, 0xF4, 0x62, 0xEE, 0xED, 0xD9,
    0xD1, 0xF2, 0xD4, 0x6B, 0xDC, 0x10, 0xE4, 0xE2,
    0x41, 0x67, 0xC4, 0x87, 0x5C, 0xF2, 0xF7, 0xA2,
    0x29, 0x7D, 0xA0, 0x2B, 0x8F, 0x4B, 0xA8, 0xE0
};

/* "abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcd"
 * (64 bytes, exactly one block)
 */
static const uint8_t tv2_msg[64] = {
    0x61,0x62,0x63,0x64, 0x61,0x62,0x63,0x64,
    0x61,0x62,0x63,0x64, 0x61,0x62,0x63,0x64,
    0x61,0x62,0x63,0x64, 0x61,0x62,0x63,0x64,
    0x61,0x62,0x63,0x64, 0x61,0x62,0x63,0x64,
    0x61,0x62,0x63,0x64, 0x61,0x62,0x63,0x64,
    0x61,0x62,0x63,0x64, 0x61,0x62,0x63,0x64,
    0x61,0x62,0x63,0x64, 0x61,0x62,0x63,0x64,
    0x61,0x62,0x63,0x64, 0x61,0x62,0x63,0x64
};
static const size_t tv2_len = 64;
static const uint8_t tv2_hash[32] = {
    0xDE, 0xBE, 0x9F, 0xF9, 0x22, 0x75, 0xB8, 0xA1,
    0x38, 0x60, 0x48, 0x89, 0xC1, 0x8E, 0x5A, 0x4D,
    0x6F, 0xDB, 0x70, 0xE5, 0x38, 0x7E, 0x57, 0x65,
    0x29, 0x3D, 0xCB, 0xA3, 0x9C, 0x0C, 0x57, 0x32
};

/* ------------------------------------------------------------------ */
/*  Helpers                                                           */
/* ------------------------------------------------------------------ */

static int hexdump_compare(const uint8_t *a, const uint8_t *b, size_t len) {
    for (size_t i = 0; i < len; i++)
        if (a[i] != b[i])
            return 0;
    return 1;
}

static void print_hash(const char *label, const uint8_t *hash) {
    printf("  %-8s: ", label);
    for (int i = 0; i < 32; i++)
        printf("%02X", hash[i]);
    printf("\n");
}

#ifdef _WIN32
static double get_time_sec(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart / (double)freq.QuadPart;
}
#else
static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
#endif

/* ------------------------------------------------------------------ */
/*  Correctness tests                                                 */
/* ------------------------------------------------------------------ */

static int test_ref(void) {
    uint8_t digest[32];
    int ok = 1;

    printf("[TEST] Reference SM3\n");

    sm3_hash(tv1_msg, tv1_len, digest);
    if (!hexdump_compare(digest, tv1_hash, 32)) {
        printf("  FAIL: tv1 (\"abc\")\n");
        print_hash("got", digest);
        print_hash("exp", tv1_hash);
        ok = 0;
    } else {
        printf("  PASS: tv1 (\"abc\")\n");
    }

    sm3_hash(tv2_msg, tv2_len, digest);
    if (!hexdump_compare(digest, tv2_hash, 32)) {
        printf("  FAIL: tv2 (64 bytes)\n");
        print_hash("got", digest);
        print_hash("exp", tv2_hash);
        ok = 0;
    } else {
        printf("  PASS: tv2 (64 bytes)\n");
    }

    /* Streaming API test */
    sm3_ctx_t ctx;
    uint8_t digest2[32];
    sm3_init(&ctx);
    sm3_update(&ctx, tv1_msg, 1);
    sm3_update(&ctx, tv1_msg + 1, 2);
    sm3_final(&ctx, digest2);
    if (!hexdump_compare(digest2, tv1_hash, 32)) {
        printf("  FAIL: streaming API\n");
        ok = 0;
    } else {
        printf("  PASS: streaming API\n");
    }

    return ok;
}

static int test_hybrid(void) {
    uint8_t digest[32];
    int ok = 1;

    printf("[TEST] Hybrid (SIMD expansion + GP compression)\n");

    sm3_hash_hybrid(tv1_msg, tv1_len, digest);
    if (!hexdump_compare(digest, tv1_hash, 32)) {
        printf("  FAIL: tv1 (\"abc\")\n");
        ok = 0;
    } else {
        printf("  PASS: tv1 (\"abc\")\n");
    }

    sm3_hash_hybrid(tv2_msg, tv2_len, digest);
    if (!hexdump_compare(digest, tv2_hash, 32)) {
        printf("  FAIL: tv2 (64 bytes)\n");
        ok = 0;
    } else {
        printf("  PASS: tv2 (64 bytes)\n");
    }

    return ok;
}

#if defined(__AVX2__)
static int test_mb_x8(void) {
    uint8_t digest[SM3_MB_X8][SM3_DIGEST_SIZE];
    const uint8_t *msgs[SM3_MB_X8];
    int ok = 1;

    printf("[TEST] AVX2 8-way multi-buffer\n");

    /* Hash "abc" 8 times */
    for (int k = 0; k < SM3_MB_X8; k++)
        msgs[k] = tv1_msg;
    sm3_mb_x8_hash(msgs, tv1_len, digest);

    for (int k = 0; k < SM3_MB_X8; k++) {
        if (!hexdump_compare(digest[k], tv1_hash, 32)) {
            printf("  FAIL: lane %d\n", k);
            ok = 0;
        }
    }
    if (ok)
        printf("  PASS: 8 × \"abc\"\n");

    /* 64-byte messages */
    for (int k = 0; k < SM3_MB_X8; k++)
        msgs[k] = tv2_msg;
    sm3_mb_x8_hash(msgs, tv2_len, digest);
    for (int k = 0; k < SM3_MB_X8; k++) {
        if (!hexdump_compare(digest[k], tv2_hash, 32)) {
            printf("  FAIL: lane %d (64-byte)\n", k);
            ok = 0;
        }
    }
    if (ok)
        printf("  PASS: 8 × 64-byte\n");

    return ok;
}
#endif

#if defined(__AVX512F__) && defined(__AVX512VL__)
static int test_mb_x16(void) {
    uint8_t digest[SM3_MB_X16][SM3_DIGEST_SIZE];
    const uint8_t *msgs[SM3_MB_X16];
    int ok = 1;

    printf("[TEST] AVX-512 16-way multi-buffer\n");

    for (int k = 0; k < SM3_MB_X16; k++)
        msgs[k] = tv1_msg;
    sm3_mb_x16_hash(msgs, tv1_len, digest);

    for (int k = 0; k < SM3_MB_X16; k++) {
        if (!hexdump_compare(digest[k], tv1_hash, 32)) {
            printf("  FAIL: lane %d\n", k);
            ok = 0;
        }
    }
    if (ok)
        printf("  PASS: 16 × \"abc\"\n");

    return ok;
}
#endif

#if defined(__aarch64__) || (defined(__ARM_NEON) && defined(__ARM_ARCH_ISA_A64))
static int test_mb_x4(void) {
    uint8_t digest[SM3_MB_X4][SM3_DIGEST_SIZE];
    const uint8_t *msgs[SM3_MB_X4];
    int ok = 1;

    printf("[TEST] ARM NEON 4-way multi-buffer\n");

    for (int k = 0; k < SM3_MB_X4; k++)
        msgs[k] = tv1_msg;
    sm3_mb_x4_hash(msgs, tv1_len, digest);

    for (int k = 0; k < SM3_MB_X4; k++) {
        if (!hexdump_compare(digest[k], tv1_hash, 32)) {
            printf("  FAIL: lane %d\n", k);
            ok = 0;
        }
    }
    if (ok)
        printf("  PASS: 4 × \"abc\"\n");

    return ok;
}
#endif

/* ------------------------------------------------------------------ */
/*  Benchmark                                                         */
/* ------------------------------------------------------------------ */

#define BENCH_ITER_SMALL   50000
#define BENCH_ITER_LARGE   2000
#define BENCH_BUF_SIZE     (1 << 20)   /* 1 MiB */

static void bench_single(const char *label,
                         void (*hash_fn)(const uint8_t *, size_t, uint8_t *),
                         size_t msg_len, int iters)
{
    uint8_t *msg = (uint8_t *)malloc(msg_len);
    uint8_t digest[32];
    double start, elapsed;
    size_t total;

    if (!msg) { printf("  %-32s malloc failed\n", label); return; }
    memset(msg, 0xAA, msg_len);

    start = get_time_sec();
    for (int i = 0; i < iters; i++)
        hash_fn(msg, msg_len, digest);
    elapsed = get_time_sec() - start;

    total = (size_t)iters * msg_len;
    double mbps = (double)total / elapsed / (1024.0 * 1024.0);

    printf("  %-32s %8.2f MiB/s  (%zu bytes × %d iters, %.3f s)\n",
           label, mbps, msg_len, iters, elapsed);

    /* Prevent optimization from eliminating the computation */
    volatile uint8_t sink = digest[0];
    (void)sink;

    free(msg);
}

#if defined(__AVX2__)
static void bench_mb_x8(const char *label, size_t msg_len, int iters) {
    uint8_t *bufs[SM3_MB_X8];
    const uint8_t *msgs[SM3_MB_X8];
    uint8_t digest[SM3_MB_X8][SM3_DIGEST_SIZE];
    double start, elapsed;
    size_t total;

    for (int k = 0; k < SM3_MB_X8; k++) {
        bufs[k] = (uint8_t *)malloc(msg_len);
        if (!bufs[k]) { printf("  malloc fail\n"); return; }
        memset(bufs[k], 0xAA, msg_len);
        msgs[k] = bufs[k];
    }

    start = get_time_sec();
    for (int i = 0; i < iters; i++) {
        for (int k = 0; k < SM3_MB_X8; k++)
            msgs[k] = bufs[k];
        sm3_mb_x8_hash(msgs, msg_len, digest);
    }
    elapsed = get_time_sec() - start;

    total = (size_t)iters * msg_len * SM3_MB_X8;
    double mbps = (double)total / elapsed / (1024.0 * 1024.0);

    printf("  %-32s %8.2f MiB/s  (%zu bytes × %d msgs × %d iters, %.3f s)\n",
           label, mbps, msg_len, SM3_MB_X8, iters, elapsed);

    volatile uint8_t sink = digest[0][0];
    (void)sink;

    for (int k = 0; k < SM3_MB_X8; k++) free(bufs[k]);
}
#endif

#if defined(__AVX512F__) && defined(__AVX512VL__)
static void bench_mb_x16(const char *label, size_t msg_len, int iters) {
    uint8_t *bufs[SM3_MB_X16];
    const uint8_t *msgs[SM3_MB_X16];
    uint8_t digest[SM3_MB_X16][SM3_DIGEST_SIZE];
    double start, elapsed;
    size_t total;

    for (int k = 0; k < SM3_MB_X16; k++) {
        bufs[k] = (uint8_t *)malloc(msg_len);
        if (!bufs[k]) { printf("  malloc fail\n"); return; }
        memset(bufs[k], 0xAA, msg_len);
        msgs[k] = bufs[k];
    }

    start = get_time_sec();
    for (int i = 0; i < iters; i++) {
        for (int k = 0; k < SM3_MB_X16; k++)
            msgs[k] = bufs[k];
        sm3_mb_x16_hash(msgs, msg_len, digest);
    }
    elapsed = get_time_sec() - start;

    total = (size_t)iters * msg_len * SM3_MB_X16;
    double mbps = (double)total / elapsed / (1024.0 * 1024.0);

    printf("  %-32s %8.2f MiB/s  (%zu bytes × %d msgs × %d iters, %.3f s)\n",
           label, mbps, msg_len, SM3_MB_X16, iters, elapsed);

    volatile uint8_t sink = digest[0][0];
    (void)sink;

    for (int k = 0; k < SM3_MB_X16; k++) free(bufs[k]);
}
#endif

#if defined(__aarch64__) || (defined(__ARM_NEON) && defined(__ARM_ARCH_ISA_A64))
static void bench_mb_x4(const char *label, size_t msg_len, int iters) {
    uint8_t *bufs[SM3_MB_X4];
    const uint8_t *msgs[SM3_MB_X4];
    uint8_t digest[SM3_MB_X4][SM3_DIGEST_SIZE];
    double start, elapsed;
    size_t total;

    for (int k = 0; k < SM3_MB_X4; k++) {
        bufs[k] = (uint8_t *)malloc(msg_len);
        if (!bufs[k]) { printf("  malloc fail\n"); return; }
        memset(bufs[k], 0xAA, msg_len);
        msgs[k] = bufs[k];
    }

    start = get_time_sec();
    for (int i = 0; i < iters; i++) {
        for (int k = 0; k < SM3_MB_X4; k++)
            msgs[k] = bufs[k];
        sm3_mb_x4_hash(msgs, msg_len, digest);
    }
    elapsed = get_time_sec() - start;

    total = (size_t)iters * msg_len * SM3_MB_X4;
    double mbps = (double)total / elapsed / (1024.0 * 1024.0);

    printf("  %-32s %8.2f MiB/s  (%zu bytes × %d msgs × %d iters, %.3f s)\n",
           label, mbps, msg_len, SM3_MB_X4, iters, elapsed);

    volatile uint8_t sink = digest[0][0];
    (void)sink;

    for (int k = 0; k < SM3_MB_X4; k++) free(bufs[k]);
}
#endif

/* ------------------------------------------------------------------ */
/*  Main                                                              */
/* ------------------------------------------------------------------ */

int main(void) {
    int all_ok = 1;
    printf("=== SM3 Software Implementation & Optimization ===\n");
    printf("Architecture: ");
#if defined(__aarch64__) || defined(_M_ARM64)
    printf("ARM64 (AArch64)\n");
#elif defined(__x86_64__) || defined(_M_X64)
    printf("x86-64\n");
#elif defined(__i386__) || defined(_M_IX86)
    printf("x86 (32-bit)\n");
#else
    printf("Unknown\n");
#endif

#if defined(__AVX512F__) && defined(__AVX512VL__)
    printf("SIMD: AVX-512 (+ AVX2 + SSE2)\n");
#elif defined(__AVX2__)
    printf("SIMD: AVX2 (+ SSE2)\n");
#elif defined(__SSE2__)
    printf("SIMD: SSE2\n");
#elif defined(__ARM_NEON)
    printf("SIMD: ARM NEON\n");
#else
    printf("SIMD: None (scalar only)\n");
#endif

    printf("\n--- Correctness Tests ---\n\n");

    if (!test_ref()) all_ok = 0;
    printf("\n");

    if (!test_hybrid()) all_ok = 0;
    printf("\n");

#if defined(__AVX2__)
    if (!test_mb_x8()) all_ok = 0;
    printf("\n");
#endif

#if defined(__AVX512F__) && defined(__AVX512VL__)
    if (!test_mb_x16()) all_ok = 0;
    printf("\n");
#endif

#if defined(__aarch64__) || (defined(__ARM_NEON) && defined(__ARM_ARCH_ISA_A64))
    if (!test_mb_x4()) all_ok = 0;
    printf("\n");
#endif

    printf("--- Performance Benchmarks ---\n\n");

    /* Small message (64 bytes = 1 block) */
    printf("  [64-byte messages]\n");
    bench_single("ref (scalar)",     sm3_hash,         64, BENCH_ITER_SMALL);
    bench_single("hybrid (SIMD+GP)", sm3_hash_hybrid,  64, BENCH_ITER_SMALL);
#if defined(__AVX2__)
    bench_mb_x8 ("AVX2 x8",          64, BENCH_ITER_SMALL / 8);
#endif
#if defined(__AVX512F__) && defined(__AVX512VL__)
    bench_mb_x16("AVX-512 x16",      64, BENCH_ITER_SMALL / 16);
#endif
#if defined(__aarch64__) || (defined(__ARM_NEON) && defined(__ARM_ARCH_ISA_A64))
    bench_mb_x4 ("NEON x4",          64, BENCH_ITER_SMALL / 4);
#endif

    /* Medium message (1 KiB) */
    printf("\n  [1024-byte messages]\n");
    bench_single("ref (scalar)",     sm3_hash,        1024, BENCH_ITER_SMALL / 4);
    bench_single("hybrid (SIMD+GP)", sm3_hash_hybrid, 1024, BENCH_ITER_SMALL / 4);
#if defined(__AVX2__)
    bench_mb_x8 ("AVX2 x8",         1024, BENCH_ITER_SMALL / 32);
#endif
#if defined(__AVX512F__) && defined(__AVX512VL__)
    bench_mb_x16("AVX-512 x16",     1024, BENCH_ITER_SMALL / 64);
#endif
#if defined(__aarch64__) || (defined(__ARM_NEON) && defined(__ARM_ARCH_ISA_A64))
    bench_mb_x4 ("NEON x4",         1024, BENCH_ITER_SMALL / 16);
#endif

    /* Large message (1 MiB) */
    printf("\n  [1 MiB messages]\n");
    bench_single("ref (scalar)",     sm3_hash,        BENCH_BUF_SIZE, BENCH_ITER_LARGE);
    bench_single("hybrid (SIMD+GP)", sm3_hash_hybrid, BENCH_BUF_SIZE, BENCH_ITER_LARGE);
#if defined(__AVX2__)
    bench_mb_x8 ("AVX2 x8",         BENCH_BUF_SIZE, BENCH_ITER_LARGE / 8);
#endif
#if defined(__AVX512F__) && defined(__AVX512VL__)
    bench_mb_x16("AVX-512 x16",     BENCH_BUF_SIZE, BENCH_ITER_LARGE / 16);
#endif
#if defined(__aarch64__) || (defined(__ARM_NEON) && defined(__ARM_ARCH_ISA_A64))
    bench_mb_x4 ("NEON x4",         BENCH_BUF_SIZE, BENCH_ITER_LARGE / 4);
#endif

    printf("\n=== %s ===\n", all_ok ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return all_ok ? 0 : 1;
}
