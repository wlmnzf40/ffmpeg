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

static av_always_inline void ComputeChromaOffset(int16x8_t samples,
                                                  int32_t coeff,
                                                  int32x4_t *low,
                                                  int32x4_t *high)
{
    const int16x8_t clipped = vmaxq_s16(vdupq_n_s16(0),
                                        vminq_s16(samples,
                                                  vdupq_n_s16(255)));
    const int32x4_t bias = vdupq_n_s32(coeff >> 9);

    *low = vsubq_s32(vshrq_n_s32(vmulq_n_s32(vmovl_s16(
                                                 vget_low_s16(clipped)),
                                             coeff),
                                 16),
                     bias);
    *high = vsubq_s32(vshrq_n_s32(vmulq_n_s32(vmovl_high_s16(clipped),
                                              coeff),
                                  16),
                      bias);
}

static av_always_inline uint8x16_t ConvertChannel(
    int32x4_t yBase0, int32x4_t yBase1,
    int32x4_t yBase2, int32x4_t yBase3,
    int32x4_t chromaLow, int32x4_t chromaHigh, int32_t yCoeff)
{
    const int32x4_t scaledLow = vmulq_n_s32(chromaLow, yCoeff);
    const int32x4_t scaledHigh = vmulq_n_s32(chromaHigh, yCoeff);
    const int16x8_t low = vcombine_s16(
        vshrn_n_s32(vaddq_s32(yBase0,
                              vzip1q_s32(scaledLow, scaledLow)), 16),
        vshrn_n_s32(vaddq_s32(yBase1,
                              vzip2q_s32(scaledLow, scaledLow)), 16));
    const int16x8_t high = vcombine_s16(
        vshrn_n_s32(vaddq_s32(yBase2,
                              vzip1q_s32(scaledHigh, scaledHigh)), 16),
        vshrn_n_s32(vaddq_s32(yBase3,
                              vzip2q_s32(scaledHigh, scaledHigh)), 16));

    return vcombine_u8(vqmovun_s16(low), vqmovun_s16(high));
}

#if defined(__GNUC__) && !defined(__clang__)
static av_always_inline void StoreRgb24Gcc(uint8_t *dest,
                                           uint8x16_t red,
                                           uint8x16_t green,
                                           uint8x16_t blue)
{
    register uint8x16_t redReg __asm__("v0") = red;
    register uint8x16_t greenReg __asm__("v1") = green;
    register uint8x16_t blueReg __asm__("v2") = blue;

    __asm__ volatile(
        "st3 {%1.16b, %2.16b, %3.16b}, [%0]"
        :
        : "r"(dest), "w"(redReg), "w"(greenReg), "w"(blueReg)
        : "memory");
}
#endif

void ff_yuv2rgb24_X_neon(SwsContext *c, const int16_t *lumFilter,
                         const int16_t **lumSrc, int lumFilterSize,
                         const int16_t *chrFilter,
                         const int16_t **chrUSrc,
                         const int16_t **chrVSrc, int chrFilterSize,
                         const int16_t **alpSrc, uint8_t *dest,
                         int dstW, int y)
{
    const int32_t yCoeff = c->rgbTableYCoeff;
    const int32x4_t tableBase = vdupq_n_s32(c->rgbTableBase);

    av_assert2(lumFilterSize == 2);
    av_assert2(chrFilterSize == 4);
    for (int x = 0; x < dstW; x += 16) {
        const int16x8_t lumaLow = filter_vertical_2_8(
            lumSrc[0] + x, lumSrc[1] + x, lumFilter[0], lumFilter[1]);
        const int16x8_t lumaHigh = filter_vertical_2_8(
            lumSrc[0] + x + 8, lumSrc[1] + x + 8,
            lumFilter[0], lumFilter[1]);
        const int16x8_t chromaU = filter_vertical_4_8(chrUSrc, x >> 1,
                                                       chrFilter);
        const int16x8_t chromaV = filter_vertical_4_8(chrVSrc, x >> 1,
                                                       chrFilter);
        int32x4_t redLow;
        int32x4_t redHigh;
        int32x4_t blueLow;
        int32x4_t blueHigh;
        int32x4_t greenULow;
        int32x4_t greenUHigh;
        int32x4_t greenVLow;
        int32x4_t greenVHigh;
        int32x4_t yBase0 = vmlaq_n_s32(tableBase,
                                       vmovl_s16(vget_low_s16(lumaLow)),
                                       yCoeff);
        int32x4_t yBase1 = vmlaq_n_s32(tableBase,
                                       vmovl_high_s16(lumaLow), yCoeff);
        int32x4_t yBase2 = vmlaq_n_s32(tableBase,
                                       vmovl_s16(vget_low_s16(lumaHigh)),
                                       yCoeff);
        int32x4_t yBase3 = vmlaq_n_s32(tableBase,
                                       vmovl_high_s16(lumaHigh), yCoeff);
        uint8x16_t red;
        uint8x16_t green;
        uint8x16_t blue;

        ComputeChromaOffset(chromaV, c->rgbTableCrv, &redLow, &redHigh);
        ComputeChromaOffset(chromaU, c->rgbTableCbu, &blueLow, &blueHigh);
        ComputeChromaOffset(chromaU, c->rgbTableCgu,
                            &greenULow, &greenUHigh);
        ComputeChromaOffset(chromaV, c->rgbTableCgv,
                            &greenVLow, &greenVHigh);

        red = ConvertChannel(yBase0, yBase1, yBase2, yBase3,
                             redLow, redHigh, yCoeff);
        green = ConvertChannel(yBase0, yBase1, yBase2, yBase3,
                               vaddq_s32(greenULow, greenVLow),
                               vaddq_s32(greenUHigh, greenVHigh),
                               yCoeff);
        blue = ConvertChannel(yBase0, yBase1, yBase2, yBase3,
                              blueLow, blueHigh, yCoeff);
#if defined(__GNUC__) && !defined(__clang__)
        StoreRgb24Gcc(dest + x * 3, red, green, blue);
#else
        {
            uint8x16x3_t rgb = { { red, green, blue } };

            vst3q_u8(dest + x * 3, rgb);
        }
#endif
    }
}
