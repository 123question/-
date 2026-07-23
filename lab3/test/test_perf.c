#define _POSIX_C_SOURCE 199309L 
#include "../include/sm4.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 高精度计时器 */
static inline double get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void print_result(const char *name, double time, size_t data_size) {
    double mbps = (double)data_size / time / (1024 * 1024);
    printf("%-20s: %8.4f s, %8.2f MB/s\n", name, time, mbps);
}

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
    
    double start, end;
    double baseline_time, ttable_time, shuffle_time;
    
    printf("\n=== SM4 Performance Test (1MB data) ===\n\n");
    
    /* Baseline */
    sm4_baseline_set_key(&ctx, key);
    start = get_time();
    for (size_t i = 0; i < NUM_BLOCKS; i++) {
        sm4_baseline_encrypt_block(&ctx, plain + i * SM4_BLOCK_SIZE,
                                   cipher + i * SM4_BLOCK_SIZE);
    }
    end = get_time();
    baseline_time = end - start;
    print_result("Baseline", baseline_time, DATA_SIZE);
    
    /* T-table */
    sm4_ttable_set_key(&ctx, key);
    start = get_time();
    for (size_t i = 0; i < NUM_BLOCKS; i++) {
        sm4_ttable_encrypt_block(&ctx, plain + i * SM4_BLOCK_SIZE,
                                 cipher + i * SM4_BLOCK_SIZE);
    }
    end = get_time();
    ttable_time = end - start;
    print_result("T-table", ttable_time, DATA_SIZE);
    printf("  Speedup: %.2fx\n", baseline_time / ttable_time);
    
    /* Shuffle */
    sm4_shuffle_set_key(&ctx, key);
    start = get_time();
    sm4_shuffle_encrypt_blocks(&ctx, plain, cipher, NUM_BLOCKS);
    end = get_time();
    shuffle_time = end - start;
    print_result("Shuffle", shuffle_time, DATA_SIZE);
    printf("  Speedup: %.2fx\n", baseline_time / shuffle_time);
    
    /* CTR */
    sm4_ctr_init(&ctx, key, iv, 0);
    start = get_time();
    sm4_ctr_encrypt(&ctx, plain, cipher, DATA_SIZE);
    end = get_time();
    print_result("CTR Mode", end - start, DATA_SIZE);
    
    free(key);
    free(iv);
    free(plain);
    free(cipher);
    
    return 0;
}
