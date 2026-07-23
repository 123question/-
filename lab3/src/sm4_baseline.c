#include "sm4_baseline.h"
#include "sm4_constants.h"

static inline uint32_t sm4_t_baseline(uint32_t x) {
    uint8_t b0 = (x >> 24) & 0xFF;
    uint8_t b1 = (x >> 16) & 0xFF;
    uint8_t b2 = (x >> 8) & 0xFF;
    uint8_t b3 = x & 0xFF;
    
    uint32_t y = ((uint32_t)SBOX[b0] << 24) | 
                 ((uint32_t)SBOX[b1] << 16) | 
                 ((uint32_t)SBOX[b2] << 8) | 
                 (uint32_t)SBOX[b3];
    
    return y ^ ROTL32(y, 2) ^ ROTL32(y, 10) ^ 
           ROTL32(y, 18) ^ ROTL32(y, 24);
}

static inline uint32_t sm4_t_prime_baseline(uint32_t x) {
    uint8_t b0 = (x >> 24) & 0xFF;
    uint8_t b1 = (x >> 16) & 0xFF;
    uint8_t b2 = (x >> 8) & 0xFF;
    uint8_t b3 = x & 0xFF;
    
    uint32_t y = ((uint32_t)SBOX[b0] << 24) | 
                 ((uint32_t)SBOX[b1] << 16) | 
                 ((uint32_t)SBOX[b2] << 8) | 
                 (uint32_t)SBOX[b3];
    
    return y ^ ROTL32(y, 13) ^ ROTL32(y, 23);
}

void sm4_baseline_set_key(SM4_Ctx *ctx, const uint8_t *key) {
    uint32_t k[4];
    k[0] = GET_UINT32_BE(key) ^ FK[0];
    k[1] = GET_UINT32_BE(key + 4) ^ FK[1];
    k[2] = GET_UINT32_BE(key + 8) ^ FK[2];
    k[3] = GET_UINT32_BE(key + 12) ^ FK[3];
    
    for (int i = 0; i < SM4_ROUNDS; i++) {
        uint32_t t = k[1] ^ k[2] ^ k[3] ^ CK[i];
        ctx->rk[i] = k[0] ^ sm4_t_prime_baseline(t);
        
        k[0] = k[1];
        k[1] = k[2];
        k[2] = k[3];
        k[3] = ctx->rk[i];
    }
    
    for (int i = 0; i < SM4_ROUNDS; i++) {
        ctx->enc_rk[i] = ctx->rk[i];
        ctx->dec_rk[i] = ctx->rk[SM4_ROUNDS - 1 - i];
    }
}

void sm4_baseline_encrypt_block(const SM4_Ctx *ctx, 
                                 const uint8_t *in, 
                                 uint8_t *out) {
    uint32_t x0 = GET_UINT32_BE(in);
    uint32_t x1 = GET_UINT32_BE(in + 4);
    uint32_t x2 = GET_UINT32_BE(in + 8);
    uint32_t x3 = GET_UINT32_BE(in + 12);
    
    for (int i = 0; i < SM4_ROUNDS; i++) {
        uint32_t t = x1 ^ x2 ^ x3 ^ ctx->enc_rk[i];
        uint32_t tmp = x0 ^ sm4_t_baseline(t);
        x0 = x1;
        x1 = x2;
        x2 = x3;
        x3 = tmp;
    }
    
    PUT_UINT32_BE(out, x3);
    PUT_UINT32_BE(out + 4, x2);
    PUT_UINT32_BE(out + 8, x1);
    PUT_UINT32_BE(out + 12, x0);
}

void sm4_baseline_decrypt_block(const SM4_Ctx *ctx, 
                                 const uint8_t *in, 
                                 uint8_t *out) {
    uint32_t x0 = GET_UINT32_BE(in);
    uint32_t x1 = GET_UINT32_BE(in + 4);
    uint32_t x2 = GET_UINT32_BE(in + 8);
    uint32_t x3 = GET_UINT32_BE(in + 12);
    
    for (int i = 0; i < SM4_ROUNDS; i++) {
        uint32_t t = x1 ^ x2 ^ x3 ^ ctx->dec_rk[i];
        uint32_t tmp = x0 ^ sm4_t_baseline(t);
        x0 = x1;
        x1 = x2;
        x2 = x3;
        x3 = tmp;
    }
    
    PUT_UINT32_BE(out, x3);
    PUT_UINT32_BE(out + 4, x2);
    PUT_UINT32_BE(out + 8, x1);
    PUT_UINT32_BE(out + 12, x0);
}
