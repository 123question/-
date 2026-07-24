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
    
    return (memcmp(plain, decrypted, 16) == 0) ? 0 : -1;
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
    
    return (memcmp(plain, decrypted, 16) == 0) ? 0 : -1;
}

static int test_shuffle(void) {
    SM4_Ctx ctx;
    uint8_t key[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
                       0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
    uint8_t plain8[8][16];
    uint8_t cipher8[8][16];
    uint8_t decrypted8[8][16];
    
    for (int i = 0; i < 8; i++) {
        memset(plain8[i], i + 1, 16);
    }
    
    sm4_shuffle_set_key(&ctx, key);
    sm4_shuffle_encrypt_blocks(&ctx, (uint8_t*)plain8, (uint8_t*)cipher8, 8);
    sm4_shuffle_decrypt_blocks(&ctx, (uint8_t*)cipher8, (uint8_t*)decrypted8, 8);
    
    return (memcmp(plain8, decrypted8, 8 * 16) == 0) ? 0 : -1;
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
    
    return (memcmp(plain, decrypted, len) == 0) ? 0 : -1;
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
    
    return (memcmp(plain, decrypted, 16) == 0) ? 0 : -1;
}
#endif

int main(void) {
    int ret = 0;
    
    printf("Testing SM4 basic encryption/decryption...\n");
    
    printf("  Baseline: ");
    printf("%s\n", test_baseline() == 0 ? "PASS" : "FAIL");
    
    printf("  T-table: ");
    printf("%s\n", test_ttable() == 0 ? "PASS" : "FAIL");
    
    printf("  Shuffle: ");
    printf("%s\n", test_shuffle() == 0 ? "PASS" : "FAIL");
    
    printf("  CTR: ");
    printf("%s\n", test_ctr() == 0 ? "PASS" : "FAIL");
    
#ifdef __AVX512F__
    printf("  AVX-512: ");
    printf("%s\n", test_avx512() == 0 ? "PASS" : "FAIL");
#else
    printf("  AVX-512: SKIP (not supported)\n");
#endif
    
    /* 汇总结果 */
    int all_pass = (test_baseline() == 0 && test_ttable() == 0 && 
                    test_shuffle() == 0 && test_ctr() == 0
#ifdef __AVX512F__
                    && test_avx512() == 0
#endif
                   );
    
    if (all_pass) {
        printf("\nAll tests passed!\n");
    } else {
        printf("\nSome tests failed!\n");
        ret = -1;
    }
    
    return ret;
}
