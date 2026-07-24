#include "../include/sm4.h"
#include "../include/sm4_avx512.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int test_baseline(void) {
    SM4_Ctx ctx;
    uint8_t key[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
                       0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
    uint8_t plain[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
                         0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
    uint8_t cipher[16], decrypted[16];
    
    sm4_baseline_set_key(&ctx, key);
    sm4_baseline_encrypt_block(&ctx, plain, cipher);
    sm4_baseline_decrypt_block(&ctx, cipher, decrypted);
    
    if (memcmp(plain, decrypted, 16) != 0) return -1;
    return 0;
}

static int test_ttable(void) {
    SM4_Ctx ctx;
    uint8_t key[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
                       0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
    uint8_t plain[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
                         0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
    uint8_t cipher[16], decrypted[16];
    
    sm4_ttable_set_key(&ctx, key);
    sm4_ttable_encrypt_block(&ctx, plain, cipher);
    sm4_ttable_decrypt_block(&ctx, cipher, decrypted);
    
    if (memcmp(plain, decrypted, 16) != 0) return -1;
    return 0;
}

static int test_shuffle(void) {
    SM4_Ctx ctx;
    uint8_t key[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
                       0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
    uint8_t plain8[8][16];
    uint8_t cipher8[8][16];
    uint8_t decrypted8[8][16];
    
    /* 初始化8个测试块 */
    for (int i = 0; i < 8; i++) {
        memset(plain8[i], i + 1, 16);
    }
    
    sm4_shuffle_set_key(&ctx, key);
    sm4_shuffle_encrypt_blocks(&ctx, (uint8_t*)plain8, (uint8_t*)cipher8, 8);
    sm4_shuffle_decrypt_blocks(&ctx, (uint8_t*)cipher8, (uint8_t*)decrypted8, 8);
    
    if (memcmp(plain8, decrypted8, 8 * 16) != 0) return -1;
    return 0;
}

static int test_ctr(void) {
    SM4_Ctx ctx;
    uint8_t key[16] = {0};
    uint8_t iv[16] = {0};
    uint8_t plain[64] = "Hello SM4! This is a CTR mode test message.";
    uint8_t cipher[64], decrypted[64];
    size_t len = strlen((char*)plain);
    
    sm4_ctr_init(&ctx, key, iv, 0);
    sm4_ctr_encrypt(&ctx, plain, cipher, len);
    
    sm4_ctr_init(&ctx, key, iv, 0);
    sm4_ctr_decrypt(&ctx, cipher, decrypted, len);
    
    if (memcmp(plain, decrypted, len) != 0) return -1;
    return 0;
}

#ifdef __AVX512F__
static int test_avx512(void) {
    SM4_Ctx ctx;
    uint8_t key[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
                       0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
    uint8_t plain[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
                         0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
    uint8_t cipher[16], decrypted[16];
    
    sm4_avx512_set_key(&ctx, key);
    sm4_avx512_encrypt_block(&ctx, plain, cipher);
    sm4_avx512_decrypt_block(&ctx, cipher, decrypted);
    
    if (memcmp(plain, decrypted, 16) != 0) return -1;
    return 0;
}

static int test_avx512_mb(void) {
    sm4_mb_x16_ctx_t mb_ctx;
    uint8_t keys[SM4_MB_X16][SM4_KEY_SIZE];
    uint8_t plains[SM4_MB_X16][SM4_BLOCK_SIZE];
    uint8_t ciphers[SM4_MB_X16][SM4_BLOCK_SIZE];
    uint8_t decrypted[SM4_MB_X16][SM4_BLOCK_SIZE];
    const uint8_t *key_ptrs[SM4_MB_X16];
    const uint8_t *in_ptrs[SM4_MB_X16];
    uint8_t *out_ptrs[SM4_MB_X16];
    uint8_t *dec_ptrs[SM4_MB_X16];
    
    for (int k = 0; k < SM4_MB_X16; k++) {
        memset(keys[k], k + 1, SM4_KEY_SIZE);
        memset(plains[k], k * 3 + 5, SM4_BLOCK_SIZE);
        key_ptrs[k] = keys[k];
        in_ptrs[k] = plains[k];
        out_ptrs[k] = ciphers[k];
        dec_ptrs[k] = decrypted[k];
    }
    
    sm4_mb_x16_set_key(&mb_ctx, key_ptrs);
    sm4_mb_x16_encrypt_blocks(&mb_ctx, in_ptrs, out_ptrs, 1);
    sm4_mb_x16_decrypt_blocks(&mb_ctx, (const uint8_t**)out_ptrs, dec_ptrs, 1);
    
    for (int k = 0; k < SM4_MB_X16; k++) {
        if (memcmp(plains[k], decrypted[k], SM4_BLOCK_SIZE) != 0) return -1;
    }
    return 0;
}
#endif /* __AVX512F__ */

int main(void) {
    int ret = 0;
    
    printf("Testing SM4 basic encryption/decryption...\n");
    
    printf("  Baseline: ");
    if (test_baseline() == 0) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
        ret = -1;
    }
    
    printf("  T-table: ");
    if (test_ttable() == 0) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
        ret = -1;
    }
    
    printf("  Shuffle: ");
    if (test_shuffle() == 0) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
        ret = -1;
    }
    
    printf("  CTR: ");
    if (test_ctr() == 0) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
        ret = -1;
    }
    
#ifdef __AVX512F__
    printf("  AVX-512: ");
    if (test_avx512() == 0 && test_avx512_mb() == 0) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
        ret = -1;
    }
#else
    printf("  AVX-512: SKIP (not supported)\n");
#endif
    
    if (ret == 0) {
        printf("\nAll tests passed!\n");
    } else {
        printf("\nSome tests failed!\n");
    }
    
    return ret;
}
