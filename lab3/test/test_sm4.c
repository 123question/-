#include "../include/sm4.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int test_sm4_basic(void) {
    printf("Testing SM4 basic encryption/decryption...\n");
    
    SM4_Ctx ctx;
    uint8_t key[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
                       0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
    uint8_t plain[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
                         0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
    uint8_t cipher[16];
    uint8_t decrypted[16];
    
    /* 测试基础实现 */
    sm4_baseline_set_key(&ctx, key);
    sm4_baseline_encrypt_block(&ctx, plain, cipher);
    sm4_baseline_decrypt_block(&ctx, cipher, decrypted);
    
    if (memcmp(plain, decrypted, 16) == 0) {
        printf("  Baseline: PASS\n");
    } else {
        printf("  Baseline: FAIL\n");
        return -1;
    }
    
    /* 测试T-table */
    sm4_ttable_set_key(&ctx, key);
    sm4_ttable_encrypt_block(&ctx, plain, cipher);
    sm4_ttable_decrypt_block(&ctx, cipher, decrypted);
    
    if (memcmp(plain, decrypted, 16) == 0) {
        printf("  T-table: PASS\n");
    } else {
        printf("  T-table: FAIL\n");
        return -1;
    }
    
    /* 测试Shuffle (需要8个块) */
    uint8_t plain8[8][16];
    uint8_t cipher8[8][16];
    uint8_t decrypted8[8][16];
    for (int i = 0; i < 8; i++) {
        memcpy(plain8[i], plain, 16);
        plain8[i][0] = i;
    }
    
    sm4_shuffle_set_key(&ctx, key);
    sm4_shuffle_encrypt_blocks(&ctx, (uint8_t*)plain8, (uint8_t*)cipher8, 8);
    sm4_shuffle_decrypt_blocks(&ctx, (uint8_t*)cipher8, (uint8_t*)decrypted8, 8);
    
    if (memcmp(plain8, decrypted8, 8 * 16) == 0) {
        printf("  Shuffle: PASS\n");
    } else {
        printf("  Shuffle: FAIL\n");
        return -1;
    }
    
    return 0;
}

int main(void) {
    if (test_sm4_basic() == 0) {
        printf("\nAll tests passed!\n");
        return 0;
    }
    return 1;
}
