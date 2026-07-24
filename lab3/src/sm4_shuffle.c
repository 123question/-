#include "sm4_shuffle.h"
#include "sm4_constants.h"
#include <string.h>

/* Shared T-table (same approach as ttable implementation) */
static uint32_t SHUFFLE_T[1024];
static int shuffle_t_inited = 0;

static void shuffle_init_t(void) {
    if (shuffle_t_inited) return;
    for (int i = 0; i < 256; i++) {
        uint32_t s = (uint32_t)SBOX[i];
        uint32_t ls = s ^ ROTL32(s, 2) ^ ROTL32(s, 10) ^
                      ROTL32(s, 18) ^ ROTL32(s, 24);
        SHUFFLE_T[i]      = ROTL32(ls, 24);
        SHUFFLE_T[256 + i] = ROTL32(ls, 16);
        SHUFFLE_T[512 + i] = ROTL32(ls, 8);
        SHUFFLE_T[768 + i] = ls;
    }
    shuffle_t_inited = 1;
}

/* Multi-block state: 8 blocks times 4 words stored in arrays for ILP/vectorization */
typedef struct {
    uint32_t x0[8];
    uint32_t x1[8];
    uint32_t x2[8];
    uint32_t x3[8];
} ShuffleState;

/* Load 8 blocks from byte arrays into state */
static void bytes_to_state(const uint8_t in[8][16], ShuffleState *state) {
    for (int i = 0; i < 8; i++) {
        state->x0[i] = GET_UINT32_BE(in[i]);
        state->x1[i] = GET_UINT32_BE(in[i] + 4);
        state->x2[i] = GET_UINT32_BE(in[i] + 8);
        state->x3[i] = GET_UINT32_BE(in[i] + 12);
    }
}

/* Store state back to 8 byte blocks with SM4 reverse order */
static void state_to_bytes_reversed(const ShuffleState *state, uint8_t out[8][16]) {
    for (int i = 0; i < 8; i++) {
        PUT_UINT32_BE(out[i],      state->x3[i]);
        PUT_UINT32_BE(out[i] + 4,  state->x2[i]);
        PUT_UINT32_BE(out[i] + 8,  state->x1[i]);
        PUT_UINT32_BE(out[i] + 12, state->x0[i]);
    }
}

/* T-table lookup for one word */
static inline uint32_t shuffle_T(uint32_t t) {
    return SHUFFLE_T[(t >> 24) & 0xFF] ^
           SHUFFLE_T[256 + ((t >> 16) & 0xFF)] ^
           SHUFFLE_T[512 + ((t >> 8) & 0xFF)] ^
           SHUFFLE_T[768 + (t & 0xFF)];
}

/* Perform one SM4 round on 8 blocks in parallel */
static void shuffle_round(ShuffleState *state, uint32_t rk) {
    uint32_t t[8], tt[8];
    for (int i = 0; i < 8; i++) {
        t[i] = state->x1[i] ^ state->x2[i] ^ state->x3[i] ^ rk;
    }
    for (int i = 0; i < 8; i++) {
        tt[i] = shuffle_T(t[i]);
    }
    for (int i = 0; i < 8; i++) {
        uint32_t tmp = state->x0[i] ^ tt[i];
        state->x0[i] = state->x1[i];
        state->x1[i] = state->x2[i];
        state->x2[i] = state->x3[i];
        state->x3[i] = tmp;
    }
}

void sm4_shuffle_set_key(SM4_Ctx *ctx, const uint8_t *key) {
    shuffle_init_t();
    sm4_baseline_set_key(ctx, key);
}

void sm4_shuffle_encrypt_blocks(const SM4_Ctx *ctx,
                                const uint8_t *in,
                                uint8_t *out,
                                size_t num_blocks) {
    shuffle_init_t();
    size_t num_batches = num_blocks / 8;
    size_t remaining   = num_blocks % 8;

    for (size_t batch = 0; batch < num_batches; batch++) {
        const uint8_t *batch_in  = in  + batch * 8 * SM4_BLOCK_SIZE;
        uint8_t        *batch_out = out + batch * 8 * SM4_BLOCK_SIZE;
        ShuffleState state;
        bytes_to_state((const uint8_t(*)[16])batch_in, &state);
        for (int round = 0; round < SM4_ROUNDS; round++) {
            shuffle_round(&state, ctx->enc_rk[round]);
        }
        state_to_bytes_reversed(&state, (uint8_t(*)[16])batch_out);
    }

    if (remaining > 0) {
        const uint8_t *rem_in  = in  + num_batches * 8 * SM4_BLOCK_SIZE;
        uint8_t        *rem_out = out + num_batches * 8 * SM4_BLOCK_SIZE;
        for (size_t i = 0; i < remaining; i++) {
            sm4_baseline_encrypt_block(ctx,
                rem_in  + i * SM4_BLOCK_SIZE,
                rem_out + i * SM4_BLOCK_SIZE);
        }
    }
}

void sm4_shuffle_decrypt_blocks(const SM4_Ctx *ctx,
                                const uint8_t *in,
                                uint8_t *out,
                                size_t num_blocks) {
    shuffle_init_t();
    size_t num_batches = num_blocks / 8;
    size_t remaining   = num_blocks % 8;

    for (size_t batch = 0; batch < num_batches; batch++) {
        const uint8_t *batch_in  = in  + batch * 8 * SM4_BLOCK_SIZE;
        uint8_t        *batch_out = out + batch * 8 * SM4_BLOCK_SIZE;
        ShuffleState state;
        bytes_to_state((const uint8_t(*)[16])batch_in, &state);
        for (int round = 0; round < SM4_ROUNDS; round++) {
            shuffle_round(&state, ctx->dec_rk[round]);
        }
        state_to_bytes_reversed(&state, (uint8_t(*)[16])batch_out);
    }

    if (remaining > 0) {
        const uint8_t *rem_in  = in  + num_batches * 8 * SM4_BLOCK_SIZE;
        uint8_t        *rem_out = out + num_batches * 8 * SM4_BLOCK_SIZE;
        for (size_t i = 0; i < remaining; i++) {
            sm4_baseline_decrypt_block(ctx,
                rem_in  + i * SM4_BLOCK_SIZE,
                rem_out + i * SM4_BLOCK_SIZE);
        }
    }
}
