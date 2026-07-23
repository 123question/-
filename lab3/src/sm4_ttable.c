#include "sm4_ttable.h"
#include "sm4_constants.h"
#include <string.h>

static uint32_t T_TABLE[1024];
static bool ttable_initialized = false;

static void init_ttable(void) {
    if (ttable_initialized) return;
    
    for (int i = 0; i < 256; i++) {
        uint32_t s = (uint32_t)SBOX[i];
        uint32_t ls = s ^ ROTL32(s, 2) ^ ROTL32(s, 10) ^ 
                      ROTL32(s, 18) ^ ROTL32(s, 24);
        
        T_TABLE[i] = ls;
        T_TABLE[256 + i] = ROTL32(ls, 8);
        T_TABLE[512 + i] = ROTL32(ls, 16);
        T_TABLE[768 + i] = ROTL32(ls, 24);
    }
    ttable_initialized = true;
}

void sm4_ttable_set_key(SM4_Ctx *ctx, const uint8_t *key) {
    init_ttable();
    sm4_baseline_set_key(ctx, key);
}

void sm4_ttable_encrypt_block(const SM4_Ctx *ctx, 
                               const uint8_t *in, 
                               uint8_t *out) {
    uint32_t x0 = GET_UINT32_BE(in);
    uint32_t x1 = GET_UINT32_BE(in + 4);
    uint32_t x2 = GET_UINT32_BE(in + 8);
    uint32_t x3 = GET_UINT32_BE(in + 12);
    
    for (int i = 0; i < SM4_ROUNDS; i++) {
        uint32_t t = x1 ^ x2 ^ x3 ^ ctx->enc_rk[i];
        
        uint32_t tt = T_TABLE[(t >> 24) & 0xFF] ^
                      T_TABLE[256 + ((t >> 16) & 0xFF)] ^
                      T_TABLE[512 + ((t >> 8) & 0xFF)] ^
                      T_TABLE[768 + (t & 0xFF)];
        
        uint32_t tmp = x0 ^ tt;
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

void sm4_ttable_decrypt_block(const SM4_Ctx *ctx, 
                               const uint8_t *in, 
                               uint8_t *out) {
    uint32_t x0 = GET_UINT32_BE(in);
    uint32_t x1 = GET_UINT32_BE(in + 4);
    uint32_t x2 = GET_UINT32_BE(in + 8);
    uint32_t x3 = GET_UINT32_BE(in + 12);
    
    for (int i = 0; i < SM4_ROUNDS; i++) {
        uint32_t t = x1 ^ x2 ^ x3 ^ ctx->dec_rk[i];
        
        uint32_t tt = T_TABLE[(t >> 24) & 0xFF] ^
                      T_TABLE[256 + ((t >> 16) & 0xFF)] ^
                      T_TABLE[512 + ((t >> 8) & 0xFF)] ^
                      T_TABLE[768 + (t & 0xFF)];
        
        uint32_t tmp = x0 ^ tt;
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
