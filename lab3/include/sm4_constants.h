#ifndef SM4_CONSTANTS_H
#define SM4_CONSTANTS_H

#include <stdint.h>

/* S盒 */
extern const uint8_t SBOX[256];

/* 密钥扩展常数 FK */
extern const uint32_t FK[4];

/* 密钥扩展常数 CK */
extern const uint32_t CK[32];

/* 辅助宏 */
#define ROTL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define ROTL8(x, n)  (((x) << (n)) | ((x) >> (8 - (n))))

#define GET_UINT32_BE(p) \
    (((uint32_t)(p)[0] << 24) | ((uint32_t)(p)[1] << 16) | \
     ((uint32_t)(p)[2] << 8) | ((uint32_t)(p)[3]))

#define PUT_UINT32_BE(p, v) \
    do { \
        (p)[0] = (uint8_t)((v) >> 24); \
        (p)[1] = (uint8_t)((v) >> 16); \
        (p)[2] = (uint8_t)((v) >> 8); \
        (p)[3] = (uint8_t)(v); \
    } while (0)

#endif /* SM4_CONSTANTS_H */
