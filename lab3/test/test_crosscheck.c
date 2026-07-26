#include "../include/sm4.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    SM4_Ctx ctx;
    uint8_t key[16] = {0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
                       0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10};
    uint8_t plain[16] = {0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
                         0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10};
    uint8_t cipher_baseline[16], cipher_ttable[16];
    uint8_t plain8[8][16], cipher8[8][16];
    int i;

    sm4_baseline_set_key(&ctx, key);
    sm4_baseline_encrypt_block(&ctx, plain, cipher_baseline);

    sm4_ttable_set_key(&ctx, key);
    sm4_ttable_encrypt_block(&ctx, plain, cipher_ttable);

    printf("Baseline: ");
    for (i=0;i<16;i++) printf("%02x", cipher_baseline[i]);
    printf("\nT-table:  ");
    for (i=0;i<16;i++) printf("%02x", cipher_ttable[i]);
    printf("\n");

    printf("Baseline==T-table: %s\n",
        memcmp(cipher_baseline, cipher_ttable, 16)==0 ? "PASS" : "FAIL");

    for (i=0;i<8;i++) memcpy(plain8[i], plain, 16);
    sm4_shuffle_set_key(&ctx, key);
    sm4_shuffle_encrypt_blocks(&ctx,(uint8_t*)plain8,(uint8_t*)cipher8,8);

    printf("Shuffle blk0: ");
    for (i=0;i<16;i++) printf("%02x", cipher8[0][i]);
    printf("\n");
    printf("Shuffle==Baseline: %s\n",
        memcmp(cipher8[0], cipher_baseline, 16)==0 ? "PASS" : "FAIL");

    sm4_shuffle_decrypt_blocks(&ctx,(uint8_t*)cipher8,(uint8_t*)plain8,8);
    int ok=1;
    for (i=0;i<8;i++) if (memcmp(plain8[i], plain, 16)!=0) ok=0;
    printf("Shuffle roundtrip: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
