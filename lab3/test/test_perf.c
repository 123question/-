#define _POSIX_C_SOURCE 199309L
#include "../include/sm4.h"
#include "../include/sm4_avx512.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static inline double get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void print_result(const char *name, double time, size_t data_size) {
    double mbps = (double)data_size / time / (1024 * 1024);
    printf("%-20s: %8.4f s, %8.2f MB/s\n", name, time, mbps);
}

/* 测试单块加密函数 */
typedef void (*encrypt_block_func_t)(const SM4_Ctx*, const uint8_t*, uint8_t*);
typedef void (*set_key_func_t)(SM4_Ctx*, const uint8_t*);

static void run_benchmark(const char *name, set_key_func_t set_key,
                          encrypt_block_func_t encrypt,
                          size_t num_blocks, uint8_t *data,
                          uint8_t *out, SM4_Ctx *ctx,
                          const uint8_t *key) {
    set_key(ctx, key);
    double start = get_time();
    for (size_t i = 0; i < num_blocks; i++) {
        encrypt(ctx, data + i * SM4_BLOCK_SIZE, out + i * SM4_BLOCK_SIZE);
    }
    double end = get_time();
    print_result(name, end - start, num_blocks * SM4_BLOCK_SIZE);
}

#ifdef __AVX512F__
static void run_benchmark_avx512_mb(const char *name,
                                    const uint8_t **in_ptrs,
                                    uint8_t **out_ptrs,
                                    size_t num_blocks,
                                    sm4_mb_x16_ctx_t *mb_ctx,
                                    const uint8_t **key_ptrs) {
    sm4_mb_x16_set_key(mb_ctx, key_ptrs);
    
    double start = get_time();
    sm4_mb_x16_encrypt_blocks(mb_ctx, in_ptrs, out_ptrs, num_blocks);
    double end = get_time();
    
    print_result(name, end - start, num_blocks * SM4_BLOCK_SIZE * SM4_MB_X16);
}
#endif

int main(void) {
    const size_t DATA_SIZE = 1024 * 1024;
    const size_t NUM_BLOCKS = DATA_SIZE / SM4_BLOCK_SIZE;
    
    uint8_t *key = calloc(SM4_KEY_SIZE, 1);
    uint8_t *iv = calloc(SM4_BLOCK_SIZE, 1);
    uint8_t *plain = malloc(DATA_SIZE);
    uint8_t *cipher = malloc(DATA_SIZE);
    SM4_Ctx ctx;
    
    if (!key || !iv || !plain || !cipher) {
        printf("Memory allocation failed!\n");
        return 1;
    }
    
    srand((unsigned)time(NULL));
    for (size_t i = 0; i < DATA_SIZE; i++) {
        plain[i] = rand() & 0xFF;
    }
    
    double baseline_time, ttable_time, shuffle_time, ctr_time;
#ifdef __AVX512F__
    double avx512_time;
#endif
    
    printf("\n=== SM4 Performance Test (1MB data) ===\n\n");
    
    /* 1. Baseline */
    run_benchmark("Baseline",
                  sm4_baseline_set_key,
                  sm4_baseline_encrypt_block,
                  NUM_BLOCKS, plain, cipher, &ctx, key);
    baseline_time = 0; /* 保存用于加速比计算 */
    
    /* 保存基准时间 */
    {
        SM4_Ctx tmp_ctx;
        sm4_baseline_set_key(&tmp_ctx, key);
        double start = get_time();
        for (size_t i = 0; i < NUM_BLOCKS; i++) {
            sm4_baseline_encrypt_block(&tmp_ctx, plain + i * SM4_BLOCK_SIZE,
                                       cipher + i * SM4_BLOCK_SIZE);
        }
        double end = get_time();
        baseline_time = end - start;
        printf("  (Baseline time: %.4f s)\n", baseline_time);
    }
    
    /* 2. T-table */
    {
        SM4_Ctx tmp_ctx;
        sm4_ttable_set_key(&tmp_ctx, key);
        double start = get_time();
        for (size_t i = 0; i < NUM_BLOCKS; i++) {
            sm4_ttable_encrypt_block(&tmp_ctx, plain + i * SM4_BLOCK_SIZE,
                                     cipher + i * SM4_BLOCK_SIZE);
        }
        double end = get_time();
        ttable_time = end - start;
        print_result("T-table", ttable_time, DATA_SIZE);
        printf("  Speedup: %.2fx\n", baseline_time / ttable_time);
    }
    
    /* 3. Shuffle */
    {
        SM4_Ctx tmp_ctx;
        sm4_shuffle_set_key(&tmp_ctx, key);
        double start = get_time();
        sm4_shuffle_encrypt_blocks(&tmp_ctx, plain, cipher, NUM_BLOCKS);
        double end = get_time();
        shuffle_time = end - start;
        print_result("Shuffle", shuffle_time, DATA_SIZE);
        printf("  Speedup: %.2fx\n", baseline_time / shuffle_time);
    }
    
    /* 4. CTR */
    {
        SM4_Ctx tmp_ctx;
        sm4_ctr_init(&tmp_ctx, key, iv, 0);
        double start = get_time();
        sm4_ctr_encrypt(&tmp_ctx, plain, cipher, DATA_SIZE);
        double end = get_time();
        ctr_time = end - start;
        print_result("CTR Mode", ctr_time, DATA_SIZE);
        printf("  Speedup: %.2fx\n", baseline_time / ctr_time);
    }
    
#ifdef __AVX512F__
    /* 5. AVX-512 单路 */
    {
        SM4_Ctx tmp_ctx;
        sm4_avx512_set_key(&tmp_ctx, key);
        double start = get_time();
        for (size_t i = 0; i < NUM_BLOCKS; i++) {
            sm4_avx512_encrypt_block(&tmp_ctx, plain + i * SM4_BLOCK_SIZE,
                                     cipher + i * SM4_BLOCK_SIZE);
        }
        double end = get_time();
        avx512_time = end - start;
        print_result("AVX-512 (single)", avx512_time, DATA_SIZE);
        printf("  Speedup: %.2fx\n", baseline_time / avx512_time);
    }
    
    /* 6. AVX-512 16路并行 */
    {
        sm4_mb_x16_ctx_t mb_ctx;
        const uint8_t *key_ptrs[SM4_MB_X16];
        const uint8_t *in_ptrs[SM4_MB_X16];
        uint8_t *out_ptrs[SM4_MB_X16];
        size_t mb_blocks = NUM_BLOCKS / SM4_MB_X16;
        
        /* 准备16路数据指针 */
        for (int k = 0; k < SM4_MB_X16; k++) {
            key_ptrs[k] = key;
            in_ptrs[k] = plain;
            out_ptrs[k] = cipher;
        }
        
        sm4_mb_x16_set_key(&mb_ctx, key_ptrs);
        double start = get_time();
        sm4_mb_x16_encrypt_blocks(&mb_ctx, in_ptrs, out_ptrs, mb_blocks);
        double end = get_time();
        double mb_time = end - start;
        print_result("AVX-512 (16-way)", mb_time, mb_blocks * SM4_BLOCK_SIZE * SM4_MB_X16);
        printf("  Speedup: %.2fx\n", baseline_time / mb_time);
    }
#else
    printf("\n  AVX-512: SKIP (not supported on this platform)\n");
#endif
    
    /* 性能总结 */
    printf("\n=== Performance Summary ===\n");
    printf("%-20s %12s %12s %12s\n", "Method", "Time(s)", "MB/s", "Speedup");
    printf("%-20s %12.4f %12.2f %12.2f\n", 
           "Baseline", baseline_time, DATA_SIZE / baseline_time / (1024 * 1024), 1.00);
    printf("%-20s %12.4f %12.2f %12.2f\n",
           "T-table", ttable_time, DATA_SIZE / ttable_time / (1024 * 1024),
           baseline_time / ttable_time);
    printf("%-20s %12.4f %12.2f %12.2f\n",
           "Shuffle", shuffle_time, DATA_SIZE / shuffle_time / (1024 * 1024),
           baseline_time / shuffle_time);
    printf("%-20s %12.4f %12.2f %12.2f\n",
           "CTR", ctr_time, DATA_SIZE / ctr_time / (1024 * 1024),
           baseline_time / ctr_time);
#ifdef __AVX512F__
    printf("%-20s %12.4f %12.2f %12.2f\n",
           "AVX-512", avx512_time, DATA_SIZE / avx512_time / (1024 * 1024),
           baseline_time / avx512_time);
#endif
    
    free(key);
    free(iv);
    free(plain);
    free(cipher);
    
    return 0;
}
