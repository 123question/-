#ifndef SM4_SHUFFLE_H
#define SM4_SHUFFLE_H

#include "sm4.h"

/* Shuffle优化实现 (比特切片, 8块并行) */
void sm4_shuffle_set_key(SM4_Ctx *ctx, const uint8_t *key);
void sm4_shuffle_encrypt_blocks(const SM4_Ctx *ctx, const uint8_t *in, 
                                 uint8_t *out, size_t num_blocks);
void sm4_shuffle_decrypt_blocks(const SM4_Ctx *ctx, const uint8_t *in, 
                                 uint8_t *out, size_t num_blocks);

#endif /* SM4_SHUFFLE_H */
