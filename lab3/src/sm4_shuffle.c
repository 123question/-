#include "sm4_shuffle.h"
#include "sm4_constants.h"
#include <string.h>

typedef struct {
    uint64_t x0[32];
    uint64_t x1[32];
    uint64_t x2[32];
    uint64_t x3[32];
} ShuffleState;

static inline uint64_t shuffle_sbox_byte(uint64_t x) {
    uint64_t result = 0;
    for (int i = 0; i < 8; i++) {
        uint8_t idx = (x >> (i * 8)) & 0xFF;
        uint64_t sval = SBOX[idx];
        result |= (sval << (i * 8));
    }
    return result;
}

static inline uint64_t shuffle_l_transform(uint64_t x) {
    uint64_t result = 0;
    for (int i = 0; i < 8; i++) {
        uint8_t byte = (x >> (i * 8)) & 0xFF;
        uint8_t lb = byte ^ ROTL8(byte, 2) ^ ROTL8(byte, 10) ^ 
                     ROTL8(byte, 18) ^ ROTL8(byte, 24);
        result |= ((uint64_t)lb << (i * 8));
    }
    return result;
}

static void bytes_to_shuffle(const uint8_t in[8][16], ShuffleState *state) {
    memset(state, 0, sizeof(ShuffleState));
    
    for (int blk = 0; blk < 8; blk++) {
        for (int byte = 0; byte < 16; byte++) {
            uint8_t val = in[blk][byte];
            for (int bit = 0; bit < 8; bit++) {
                if (val & (1 << bit)) {
                    int pos = byte * 8 + bit;
                    state->x0[pos] |= (1ULL << blk);
                }
            }
        }
    }
}

static void shuffle_to_bytes(const ShuffleState *state, uint8_t out[8][16]) {
    memset(out, 0, 8 * 16);
    
    for (int blk = 0; blk < 8; blk++) {
        for (int byte = 0; byte < 16; byte++) {
            uint8_t val = 0;
            for (int bit = 0; bit < 8; bit++) {
                int pos = byte * 8 + bit;
                if (state->x0[pos] & (1ULL << blk)) {
                    val |= (1 << bit);
                }
            }
            out[blk][byte] = val;
        }
    }
}

void sm4_shuffle_set_key(SM4_Ctx *ctx, const uint8_t *key) {
    sm4_baseline_set_key(ctx, key);
}

void sm4_shuffle_encrypt_blocks(const SM4_Ctx *ctx, 
                                 const uint8_t *in, 
                                 uint8_t *out, 
                                 size_t num_blocks) {
    size_t num_batches = num_blocks / 8;
    size_t remaining = num_blocks % 8;
    
    for (size_t batch = 0; batch < num_batches; batch++) {
        const uint8_t *batch_in = in + batch * 8 * SM4_BLOCK_SIZE;
        uint8_t *batch_out = out + batch * 8 * SM4_BLOCK_SIZE;
        
        ShuffleState state;
        bytes_to_shuffle((const uint8_t(*)[16])batch_in, &state);
        
        for (int round = 0; round < SM4_ROUNDS; round++) {
            uint32_t rk = ctx->enc_rk[round];
            
            for (int word = 0; word < 4; word++) {
                uint64_t t = state.x1[word] ^ state.x2[word] ^ 
                             state.x3[word] ^ rk;
                uint64_t s = shuffle_sbox_byte(t);
                uint64_t l = shuffle_l_transform(s);
                state.x0[word] ^= l;
            }
            
            for (int word = 0; word < 4; word++) {
                uint64_t tmp = state.x0[word];
                state.x0[word] = state.x1[word];
                state.x1[word] = state.x2[word];
                state.x2[word] = state.x3[word];
                state.x3[word] = tmp;
            }
        }
        
        shuffle_to_bytes(&state, (uint8_t(*)[16])batch_out);
    }
    
    if (remaining > 0) {
        const uint8_t *rem_in = in + num_batches * 8 * SM4_BLOCK_SIZE;
        uint8_t *rem_out = out + num_batches * 8 * SM4_BLOCK_SIZE;
        for (size_t i = 0; i < remaining; i++) {
            sm4_baseline_encrypt_block(ctx, rem_in + i * SM4_BLOCK_SIZE,
                                       rem_out + i * SM4_BLOCK_SIZE);
        }
    }
}

void sm4_shuffle_decrypt_blocks(const SM4_Ctx *ctx, 
                                 const uint8_t *in, 
                                 uint8_t *out, 
                                 size_t num_blocks) {
    size_t num_batches = num_blocks / 8;
    size_t remaining = num_blocks % 8;
    
    for (size_t batch = 0; batch < num_batches; batch++) {
        const uint8_t *batch_in = in + batch * 8 * SM4_BLOCK_SIZE;
        uint8_t *batch_out = out + batch * 8 * SM4_BLOCK_SIZE;
        
        ShuffleState state;
        bytes_to_shuffle((const uint8_t(*)[16])batch_in, &state);
        
        for (int round = 0; round < SM4_ROUNDS; round++) {
            uint32_t rk = ctx->dec_rk[round];
            
            for (int word = 0; word < 4; word++) {
                uint64_t t = state.x1[word] ^ state.x2[word] ^ 
                             state.x3[word] ^ rk;
                uint64_t s = shuffle_sbox_byte(t);
                uint64_t l = shuffle_l_transform(s);
                state.x0[word] ^= l;
            }
            
            for (int word = 0; word < 4; word++) {
                uint64_t tmp = state.x0[word];
                state.x0[word] = state.x1[word];
                state.x1[word] = state.x2[word];
                state.x2[word] = state.x3[word];
                state.x3[word] = tmp;
            }
        }
        
        shuffle_to_bytes(&state, (uint8_t(*)[16])batch_out);
    }
    
    if (remaining > 0) {
        const uint8_t *rem_in = in + num_batches * 8 * SM4_BLOCK_SIZE;
        uint8_t *rem_out = out + num_batches * 8 * SM4_BLOCK_SIZE;
        for (size_t i = 0; i < remaining; i++) {
            sm4_baseline_decrypt_block(ctx, rem_in + i * SM4_BLOCK_SIZE,
                                       rem_out + i * SM4_BLOCK_SIZE);
        }
    }
}
