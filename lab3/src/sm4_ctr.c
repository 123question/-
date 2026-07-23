#include "sm4_ctr.h"
#include "sm4_baseline.h"
#include <string.h>

void sm4_ctr_init(SM4_Ctx *ctx, const uint8_t *key, 
                   const uint8_t *iv, uint64_t counter) {
    sm4_baseline_set_key(ctx, key);
    memcpy(ctx->iv, iv, SM4_BLOCK_SIZE);
    ctx->counter = counter;
}

static void sm4_ctr_crypt(SM4_Ctx *ctx, const uint8_t *in, 
                           uint8_t *out, size_t length) {
    uint8_t counter_block[SM4_BLOCK_SIZE];
    uint8_t keystream[SM4_BLOCK_SIZE];
    size_t processed = 0;
    
    while (processed < length) {
        memcpy(counter_block, ctx->iv, SM4_BLOCK_SIZE);
        uint64_t ctr = ctx->counter;
        for (int i = 7; i >= 0; i--) {
            counter_block[SM4_BLOCK_SIZE - 8 + i] = (uint8_t)(ctr & 0xFF);
            ctr >>= 8;
        }
        
        sm4_baseline_encrypt_block(ctx, counter_block, keystream);
        
        size_t remaining = length - processed;
        size_t chunk = (remaining < SM4_BLOCK_SIZE) ? remaining : SM4_BLOCK_SIZE;
        
        for (size_t i = 0; i < chunk; i++) {
            out[processed + i] = in[processed + i] ^ keystream[i];
        }
        
        processed += chunk;
        ctx->counter++;
    }
}

void sm4_ctr_encrypt(SM4_Ctx *ctx, const uint8_t *in, 
                      uint8_t *out, size_t length) {
    sm4_ctr_crypt(ctx, in, out, length);
}

void sm4_ctr_decrypt(SM4_Ctx *ctx, const uint8_t *in, 
                      uint8_t *out, size_t length) {
    sm4_ctr_crypt(ctx, in, out, length);
}
