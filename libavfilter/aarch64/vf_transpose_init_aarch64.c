/*
 * Copyright (c) 2026 Wang Limin
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <arm_neon.h>
#include <stddef.h>
#include <stdint.h>

#include "libavutil/attributes.h"
#include "libavutil/cpu.h"
#include "libavutil/aarch64/cpu.h"
#include "libavfilter/transpose.h"

void ff_transpose_8x8_16_neon(uint8_t *src, ptrdiff_t srcLinesize,
                              uint8_t *dst, ptrdiff_t dstLinesize);
void ff_transpose_8x8_24_neon(uint8_t *src, ptrdiff_t srcLinesize,
                              uint8_t *dst, ptrdiff_t dstLinesize);

static inline void transpose_8x8_u8(const uint8x8_t input[8], uint8x8_t output[8])
{
    uint8x8x2_t bytes[4];
    uint16x4x2_t halfwords[4];
    uint32x2x2_t words[4];

    bytes[0] = vtrn_u8(input[0], input[1]);
    bytes[1] = vtrn_u8(input[2], input[3]);
    bytes[2] = vtrn_u8(input[4], input[5]);
    bytes[3] = vtrn_u8(input[6], input[7]);

    halfwords[0] = vtrn_u16(vreinterpret_u16_u8(bytes[0].val[0]),
                             vreinterpret_u16_u8(bytes[1].val[0]));
    halfwords[1] = vtrn_u16(vreinterpret_u16_u8(bytes[0].val[1]),
                             vreinterpret_u16_u8(bytes[1].val[1]));
    halfwords[2] = vtrn_u16(vreinterpret_u16_u8(bytes[2].val[0]),
                             vreinterpret_u16_u8(bytes[3].val[0]));
    halfwords[3] = vtrn_u16(vreinterpret_u16_u8(bytes[2].val[1]),
                             vreinterpret_u16_u8(bytes[3].val[1]));

    words[0] = vtrn_u32(vreinterpret_u32_u16(halfwords[0].val[0]),
                         vreinterpret_u32_u16(halfwords[2].val[0]));
    words[1] = vtrn_u32(vreinterpret_u32_u16(halfwords[1].val[0]),
                         vreinterpret_u32_u16(halfwords[3].val[0]));
    words[2] = vtrn_u32(vreinterpret_u32_u16(halfwords[0].val[1]),
                         vreinterpret_u32_u16(halfwords[2].val[1]));
    words[3] = vtrn_u32(vreinterpret_u32_u16(halfwords[1].val[1]),
                         vreinterpret_u32_u16(halfwords[3].val[1]));

    output[0] = vreinterpret_u8_u32(words[0].val[0]);
    output[1] = vreinterpret_u8_u32(words[1].val[0]);
    output[2] = vreinterpret_u8_u32(words[2].val[0]);
    output[3] = vreinterpret_u8_u32(words[3].val[0]);
    output[4] = vreinterpret_u8_u32(words[0].val[1]);
    output[5] = vreinterpret_u8_u32(words[1].val[1]);
    output[6] = vreinterpret_u8_u32(words[2].val[1]);
    output[7] = vreinterpret_u8_u32(words[3].val[1]);
}

static void transpose_8x8_8_neon(uint8_t *src, ptrdiff_t srcLinesize,
                                 uint8_t *dst, ptrdiff_t dstLinesize)
{
    uint8x8_t input[8];
    uint8x8_t output[8];

    for (int row = 0; row < 8; row++) {
        input[row] = vld1_u8(src + row * srcLinesize);
    }
    transpose_8x8_u8(input, output);
    for (int row = 0; row < 8; row++) {
        vst1_u8(dst + row * dstLinesize, output[row]);
    }
}

av_cold void ff_transpose_init_aarch64(TransVtable *v, int pixelStep)
{
    if (!have_neon(av_get_cpu_flags())) {
        return;
    }

    if (pixelStep == 1) {
        v->transpose_8x8 = transpose_8x8_8_neon;
    } else if (pixelStep == 2) {
        v->transpose_8x8 = ff_transpose_8x8_16_neon;
    } else if (pixelStep == 3) {
        v->transpose_8x8 = ff_transpose_8x8_24_neon;
    }
}
