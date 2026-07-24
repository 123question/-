/* sm4_avx512.c — AVX-512 multi-buffer SM4 (16-way parallel)
 *
 * Processes 16 independent equal-length messages simultaneously using
 * 512-bit ZMM registers. Leverages AVX-512F native VPROLD for efficient
 * 32-bit lane rotation without shift+or emulation.
 *
 * Build requirement: -mavx512f -mavx512vl
 */

#include "sm4_avx512.h"

#if defined(__AVX512F__) && defined(__AVX512VL__)

#include <string.h>
#include <immintrin.h>

/* ================================================================
 *  SIMD helpers — AVX-512 has native VPROLD for 32-bit rotate
 * ================================================================ */

#define mm512_rotl32(x, n)  _mm512_rol_epi32(x, n)

/* SM4 轮函数 T = L(Sbox(x)) */
static inline __m512i sm4_t_512(__m512i x) {
    /* 提取4个字节 */
    __m512i b0, b1, b2, b3;
    __m512i mask = _mm512_set1_epi32(0xFF);
    
    b0 = _mm512_and_si512(_mm512_srli_epi32(x, 24), mask);
    b1 = _mm512_and_si512(_mm512_srli_epi32(x, 16), mask);
    b2 = _mm512_and_si512(_mm512_srli_epi32(x, 8), mask);
    b3 = _mm512_and_si512(x, mask);
    
    /* S盒查表 — 使用pshufb模拟，需要分别处理每个字节 */
    /* 简化：使用标量查表 + 广播，或使用AVX512-VBMI的vpermw */
    /* 这里提供标量兼容版本，实际可用vpermi2b等指令优化 */
    uint32_t vals[16];
    _mm512_storeu_si512(vals, x);
    
    uint32_t result[16];
    for (int i = 0; i < 16; i++) {
        uint32_t v = vals[i];
        uint8_t s0 = SM4_SBOX[(v >> 24) & 0xFF];
        uint8_t s1 = SM4_SBOX[(v >> 16) & 0xFF];
        uint8_t s2 = SM4_SBOX[(v >> 8) & 0xFF];
        uint8_t s3 = SM4_SBOX[v & 0xFF];
        result[i] = ((uint32_t)s0 << 24) | ((uint32_t)s1 << 16) |
                    ((uint32_t)s2 << 8) | (uint32_t)s3;
    }
    
    __m512i y = _mm512_loadu_si512(result);
    
    /* 线性变换 L */
    return _mm512_xor_si512(
        _mm512_xor_si512(
            _mm512_xor_si512(
                _mm512_xor_si512(y, mm512_rotl32(y, 2)),
                mm512_rotl32(y, 10)),
            mm512_rotl32(y, 18)),
        mm512_rotl32(y, 24));
}

/* 密钥扩展线性变换 L' */
static inline __m512i sm4_tp_512(__m512i x) {
    uint32_t vals[16];
    _mm512_storeu_si512(vals, x);
    
    uint32_t result[16];
    for (int i = 0; i < 16; i++) {
        uint32_t v = vals[i];
        uint8_t s0 = SM4_SBOX[(v >> 24) & 0xFF];
        uint8_t s1 = SM4_SBOX[(v >> 16) & 0xFF];
        uint8_t s2 = SM4_SBOX[(v >> 8) & 0xFF];
        uint8_t s3 = SM4_SBOX[v & 0xFF];
        result[i] = ((uint32_t)s0 << 24) | ((uint32_t)s1 << 16) |
                    ((uint32_t)s2 << 8) | (uint32_t)s3;
    }
    
    __m512i y = _mm512_loadu_si512(result);
    
    /* L' 线性变换 */
    return _mm512_xor_si512(
        _mm512_xor_si512(y, mm512_rotl32(y, 13)),
        mm512_rotl32(y, 23));
}

/* ================================================================
 *  加载16个块到 SoA 格式
 * ================================================================ */

static void load_transpose_x16(const uint8_t *buf[SM4_MB_X16],
                                __m512i X[4]) {
    for (int j = 0; j < 4; j++) {
        uint32_t vals[SM4_MB_X16];
        for (int k = 0; k < SM4_MB_X16; k++) {
            const uint8_t *b = buf[k] + j * 4;
            vals[k] = ((uint32_t)b[0] << 24) |
                      ((uint32_t)b[1] << 16) |
                      ((uint32_t)b[2] << 8) |
                      ((uint32_t)b[3]);
        }
        X[j] = _mm512_loadu_si512(vals);
    }
}

static void store_transpose_x16(const __m512i X[4], uint8_t *out[SM4_MB_X16]) {
    for (int j = 0; j < 4; j++) {
        uint32_t vals[SM4_MB_X16];
        _mm512_storeu_si512(vals, X[j]);
        for (int k = 0; k < SM4_MB_X16; k++) {
            uint8_t *b = out[k] + j * 4;
            uint32_t v = vals[k];
            b[0] = (uint8_t)(v >> 24);
            b[1] = (uint8_t)(v >> 16);
            b[2] = (uint8_t)(v >> 8);
            b[3] = (uint8_t)v;
        }
    }
}

/* ================================================================
 *  密钥扩展 — 16路并行
 * ================================================================ */

static const uint32_t SM4_FK[4] = {
    0xa3b1bac6, 0x56aa3350, 0x677d9197, 0xb27022dc
};

static const uint32_t SM4_CK[32] = {
    0x00070e15, 0x1c232a31, 0x383f464d, 0x545b6269,
    0x70777e85, 0x8c939aa1, 0xa8afb6bd, 0xc4cbd2d9,
    0xe0e7eef5, 0xfc030a11, 0x181f262d, 0x343b4249,
    0x50575e65, 0x6c737a81, 0x888f969d, 0xa4abb2b9,
    0xc0c7ced5, 0xdce3eaf1, 0xf8ff060d, 0x141b2229,
    0x30373e45, 0x4c535a61, 0x686f767d, 0x848b9299,
    0xa0a7aeb5, 0xbcc3cad1, 0xd8dfe6ed, 0xf4fb0209,
    0x10171e25, 0x2c333a41, 0x484f565d, 0x646b7279
};

/* 16路并行密钥扩展 */
void sm4_mb_x16_set_key(sm4_mb_x16_ctx_t *ctx, const uint8_t *key[SM4_MB_X16]) {
    __m512i K[4];
    __m512i rk[32];
    
    /* 加载16个密钥并异或FK */
    for (int i = 0; i < 4; i++) {
        uint32_t vals[SM4_MB_X16];
        for (int k = 0; k < SM4_MB_X16; k++) {
            const uint8_t *b = key[k] + i * 4;
            vals[k] = ((uint32_t)b[0] << 24) |
                      ((uint32_t)b[1] << 16) |
                      ((uint32_t)b[2] << 8) |
                      ((uint32_t)b[3]);
        }
        K[i] = _mm512_xor_si512(
            _mm512_loadu_si512(vals),
            _mm512_set1_epi32(SM4_FK[i]));
    }
    
    /* 生成32轮密钥 */
    for (int round = 0; round < 32; round++) {
        __m512i t = _mm512_xor_si512(
            _mm512_xor_si512(K[1], K[2]),
            _mm512_xor_si512(K[3], _mm512_set1_epi32(SM4_CK[round])));
        
        rk[round] = _mm512_xor_si512(K[0], sm4_tp_512(t));
        
        K[0] = K[1];
        K[1] = K[2];
        K[2] = K[3];
        K[3] = rk[round];
    }
    
    /* 存储轮密钥到上下文 */
    for (int round = 0; round < 32; round++) {
        _mm512_storeu_si512((uint32_t*)&ctx->state[round * 16], rk[round]);
    }
}

/* ================================================================
 *  加密/解密 — 16路并行
 * ================================================================ */

void sm4_mb_x16_encrypt_blocks(const sm4_mb_x16_ctx_t *ctx,
                                const uint8_t *in[SM4_MB_X16],
                                uint8_t *out[SM4_MB_X16],
                                size_t num_blocks) {
    __m512i X[4];
    __m512i rk[32];
    
    /* 加载轮密钥 */
    for (int round = 0; round < 32; round++) {
        rk[round] = _mm512_loadu_si512((const uint32_t*)&ctx->state[round * 16]);
    }
    
    for (size_t blk = 0; blk < num_blocks; blk++) {
        const uint8_t *in_ptr[SM4_MB_X16];
        uint8_t *out_ptr[SM4_MB_X16];
        
        for (int k = 0; k < SM4_MB_X16; k++) {
            in_ptr[k] = in[k] + blk * SM4_BLOCK_SIZE;
            out_ptr[k] = out[k] + blk * SM4_BLOCK_SIZE;
        }
        
        /* 加载16个块 */
        load_transpose_x16(in_ptr, X);
        
        /* 32轮加密 */
        for (int round = 0; round < 32; round++) {
            __m512i t = _mm512_xor_si512(
                _mm512_xor_si512(X[1], X[2]),
                _mm512_xor_si512(X[3], rk[round]));
            
            __m512i tmp = _mm512_xor_si512(X[0], sm4_t_512(t));
            
            X[0] = X[1];
            X[1] = X[2];
            X[2] = X[3];
            X[3] = tmp;
        }
        
        /* 反序输出 */
        __m512i tmp0 = X[0], tmp1 = X[1], tmp2 = X[2], tmp3 = X[3];
        X[0] = tmp3;
        X[1] = tmp2;
        X[2] = tmp1;
        X[3] = tmp0;
        
        store_transpose_x16(X, out_ptr);
    }
}

void sm4_mb_x16_decrypt_blocks(const sm4_mb_x16_ctx_t *ctx,
                                const uint8_t *in[SM4_MB_X16],
                                uint8_t *out[SM4_MB_X16],
                                size_t num_blocks) {
    __m512i X[4];
    __m512i rk[32];
    
    /* 加载轮密钥（反向） */
    for (int round = 0; round < 32; round++) {
        rk[round] = _mm512_loadu_si512((const uint32_t*)&ctx->state[(31 - round) * 16]);
    }
    
    for (size_t blk = 0; blk < num_blocks; blk++) {
        const uint8_t *in_ptr[SM4_MB_X16];
        uint8_t *out_ptr[SM4_MB_X16];
        
        for (int k = 0; k < SM4_MB_X16; k++) {
            in_ptr[k] = in[k] + blk * SM4_BLOCK_SIZE;
            out_ptr[k] = out[k] + blk * SM4_BLOCK_SIZE;
        }
        
        load_transpose_x16(in_ptr, X);
        
        /* 32轮解密 */
        for (int round = 0; round < 32; round++) {
            __m512i t = _mm512_xor_si512(
                _mm512_xor_si512(X[1], X[2]),
                _mm512_xor_si512(X[3], rk[round]));
            
            __m512i tmp = _mm512_xor_si512(X[0], sm4_t_512(t));
            
            X[0] = X[1];
            X[1] = X[2];
            X[2] = X[3];
            X[3] = tmp;
        }
        
        __m512i tmp0 = X[0], tmp1 = X[1], tmp2 = X[2], tmp3 = X[3];
        X[0] = tmp3;
        X[1] = tmp2;
        X[2] = tmp1;
        X[3] = tmp0;
        
        store_transpose_x16(X, out_ptr);
    }
}

/* ================================================================
 *  单路兼容接口
 * ================================================================ */

void sm4_avx512_set_key(SM4_Ctx *ctx, const uint8_t *key) {
    const uint8_t *keys[SM4_MB_X16];
    sm4_mb_x16_ctx_t mb_ctx;
    
    for (int k = 0; k < SM4_MB_X16; k++) {
        keys[k] = key;
    }
    
    sm4_mb_x16_set_key(&mb_ctx, keys);
    
    /* 提取第一路的轮密钥 */
    for (int round = 0; round < 32; round++) {
        ctx->enc_rk[round] = ((uint32_t*)&mb_ctx.state[round * 16])[0];
        ctx->dec_rk[round] = ctx->enc_rk[31 - round];
    }
}

void sm4_avx512_encrypt_block(const SM4_Ctx *ctx, const uint8_t *in, uint8_t *out) {
    const uint8_t *in_ptr[SM4_MB_X16];
    uint8_t *out_ptr[SM4_MB_X16];
    sm4_mb_x16_ctx_t mb_ctx;
    
    /* 将单路密钥转换为16路格式 */
    for (int round = 0; round < 32; round++) {
        uint32_t val = ctx->enc_rk[round];
        for (int k = 0; k < SM4_MB_X16; k++) {
            ((uint32_t*)&mb_ctx.state[round * 16])[k] = val;
        }
    }
    
    for (int k = 0; k < SM4_MB_X16; k++) {
        in_ptr[k] = in;
        out_ptr[k] = out;
    }
    
    sm4_mb_x16_encrypt_blocks(&mb_ctx, in_ptr, out_ptr, 1);
}

void sm4_avx512_decrypt_block(const SM4_Ctx *ctx, const uint8_t *in, uint8_t *out) {
    const uint8_t *in_ptr[SM4_MB_X16];
    uint8_t *out_ptr[SM4_MB_X16];
    sm4_mb_x16_ctx_t mb_ctx;
    
    for (int round = 0; round < 32; round++) {
        uint32_t val = ctx->dec_rk[round];
        for (int k = 0; k < SM4_MB_X16; k++) {
            ((uint32_t*)&mb_ctx.state[round * 16])[k] = val;
        }
    }
    
    for (int k = 0; k < SM4_MB_X16; k++) {
        in_ptr[k] = in;
        out_ptr[k] = out;
    }
    
    sm4_mb_x16_decrypt_blocks(&mb_ctx, in_ptr, out_ptr, 1);
}

#endif /* __AVX512F__ && __AVX512VL__ */
