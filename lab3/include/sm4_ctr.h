#ifndef SM4_CTR_H
#define SM4_CTR_H

#include "sm4.h"

/* CTR工作模式 */
void sm4_ctr_init(SM4_Ctx *ctx, const uint8_t *key, 
                  const uint8_t *iv, uint64_t counter);
void sm4_ctr_encrypt(SM4_Ctx *ctx, const uint8_t *in, uint8_t *out, size_t length);
void sm4_ctr_decrypt(SM4_Ctx *ctx, const uint8_t *in, uint8_t *out, size_t length);

#endif /* SM4_CTR_H */
