/*
 * AArch64 NEON packed RGB output
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <arm_neon.h>
#include <stdint.h>

#include "libavutil/mem_internal.h"
#include "libswscale/swscale_internal.h"

static av_always_inline int16x4_t filter_vertical_2(const int16_t *first,
                                                     const int16_t *second,
                                                     int16_t coeff0,
                                                     int16_t coeff1)
{
    int32x4_t sum = vdupq_n_s32(1 << 18);

    sum = vmlal_n_s16(sum, vld1_s16(first), coeff0);
    sum = vmlal_n_s16(sum, vld1_s16(second), coeff1);
    return vmovn_s32(vshrq_n_s32(sum, 19));
}

static av_always_inline int16x4_t filter_vertical_4(const int16_t **src,
                                                     int offset,
                                                     const int16_t *filter)
{
    int32x4_t sum = vdupq_n_s32(1 << 18);

    sum = vmlal_n_s16(sum, vld1_s16(src[0] + offset), filter[0]);
    sum = vmlal_n_s16(sum, vld1_s16(src[1] + offset), filter[1]);
    sum = vmlal_n_s16(sum, vld1_s16(src[2] + offset), filter[2]);
    sum = vmlal_n_s16(sum, vld1_s16(src[3] + offset), filter[3]);
    return vmovn_s32(vshrq_n_s32(sum, 19));
}

static av_always_inline int16x8_t filter_vertical_2_8(const int16_t *first,
                                                       const int16_t *second,
                                                       int16_t coeff0,
                                                       int16_t coeff1)
{
    int16x8_t firstPixels = vld1q_s16(first);
    int16x8_t secondPixels = vld1q_s16(second);
    int32x4_t low = vdupq_n_s32(1 << 18);
    int32x4_t high = vdupq_n_s32(1 << 18);

    low = vmlal_n_s16(low, vget_low_s16(firstPixels), coeff0);
    high = vmlal_n_s16(high, vget_high_s16(firstPixels), coeff0);
    low = vmlal_n_s16(low, vget_low_s16(secondPixels), coeff1);
    high = vmlal_n_s16(high, vget_high_s16(secondPixels), coeff1);
    return vcombine_s16(vmovn_s32(vshrq_n_s32(low, 19)),
                        vmovn_s32(vshrq_n_s32(high, 19)));
}

static av_always_inline int16x8_t filter_vertical_4_8(const int16_t **src,
                                                       int offset,
                                                       const int16_t *filter)
{
    int32x4_t low = vdupq_n_s32(1 << 18);
    int32x4_t high = vdupq_n_s32(1 << 18);

    for (int tap = 0; tap < 4; tap++) {
        int16x8_t pixels = vld1q_s16(src[tap] + offset);

        low = vmlal_n_s16(low, vget_low_s16(pixels), filter[tap]);
        high = vmlal_n_s16(high, vget_high_s16(pixels), filter[tap]);
    }
    return vcombine_s16(vmovn_s32(vshrq_n_s32(low, 19)),
                        vmovn_s32(vshrq_n_s32(high, 19)));
}

void ff_yuv2rgb24_X_neon(SwsContext *c, const int16_t *lumFilter,
                         const int16_t **lumSrc, int lumFilterSize,
                         const int16_t *chrFilter,
                         const int16_t **chrUSrc,
                         const int16_t **chrVSrc, int chrFilterSize,
                         const int16_t **alpSrc, uint8_t *dest,
                         int dstW, int y)
{
    DECLARE_ALIGNED(16, int16_t, luma)[16];
    DECLARE_ALIGNED(16, int16_t, chromaU)[8];
    DECLARE_ALIGNED(16, int16_t, chromaV)[8];

    av_assert2(lumFilterSize == 2);
    av_assert2(chrFilterSize == 4);
    for (int x = 0; x < dstW; x += 16) {
        vst1q_s16(luma, filter_vertical_2_8(lumSrc[0] + x,
                                            lumSrc[1] + x,
                                            lumFilter[0], lumFilter[1]));
        vst1q_s16(luma + 8, filter_vertical_2_8(lumSrc[0] + x + 8,
                                                lumSrc[1] + x + 8,
                                                lumFilter[0], lumFilter[1]));
        vst1q_s16(chromaU,
                  filter_vertical_4_8(chrUSrc, x >> 1, chrFilter));
        vst1q_s16(chromaV,
                  filter_vertical_4_8(chrVSrc, x >> 1, chrFilter));

        for (int pair = 0; pair < 8; pair++) {
            const int y0 = luma[pair * 2];
            const int y1 = luma[pair * 2 + 1];
            const int u = chromaU[pair];
            const int v = chromaV[pair];
            const uint8_t *red = c->table_rV[v + YUVRGB_TABLE_HEADROOM];
            const uint8_t *green = c->table_gU[u + YUVRGB_TABLE_HEADROOM] +
                                   c->table_gV[v + YUVRGB_TABLE_HEADROOM];
            const uint8_t *blue = c->table_bU[u + YUVRGB_TABLE_HEADROOM];
            uint8_t *rgb = dest + x * 3 + pair * 6;

            rgb[0] = red[y0];
            rgb[1] = green[y0];
            rgb[2] = blue[y0];
            rgb[3] = red[y1];
            rgb[4] = green[y1];
            rgb[5] = blue[y1];
        }
    }
}
