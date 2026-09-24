/*
 * AArch64 NEON HEVC 10-bit quarter-pixel interpolation
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

#include "libavutil/common.h"
#include "libavutil/mem_internal.h"
#include "libavcodec/hevc/dsp.h"

#define MAX_PB_SIZE 64
#define QPEL_EXTRA 7
#define QPEL_EXTRA_BEFORE 3
#define EPEL_EXTRA 3
#define EPEL_EXTRA_BEFORE 1

typedef struct FilterAccumulator {
    int32x4_t low;
    int32x4_t high;
} FilterAccumulator;

void ff_hevc_qpel_10_filter_horizontal_neon(int16_t *dst,
                                             const uint16_t *src,
                                             ptrdiff_t srcStride, int rows,
                                             int width, int dstStride,
                                             const int8_t *filter);
void ff_hevc_put_hevc_qpel_hv_10_neon(int16_t *dst, const uint8_t *srcBytes,
                                       ptrdiff_t srcStrideBytes, int height,
                                       intptr_t mx, intptr_t my, int width);
void ff_hevc_put_hevc_qpel_h_10_neon(int16_t *dst, const uint8_t *srcBytes,
                                      ptrdiff_t srcStrideBytes, int height,
                                      intptr_t mx, intptr_t my, int width);
void ff_hevc_put_hevc_qpel_v_10_neon(int16_t *dst, const uint8_t *srcBytes,
                                      ptrdiff_t srcStrideBytes, int height,
                                      intptr_t mx, intptr_t my, int width);
void ff_hevc_put_hevc_qpel_uni_h_10_neon(uint8_t *dstBytes,
                                          ptrdiff_t dstStrideBytes,
                                          const uint8_t *srcBytes,
                                          ptrdiff_t srcStrideBytes, int height,
                                          intptr_t mx, intptr_t my, int width);
void ff_hevc_put_hevc_qpel_uni_v_10_neon(uint8_t *dstBytes,
                                          ptrdiff_t dstStrideBytes,
                                          const uint8_t *srcBytes,
                                          ptrdiff_t srcStrideBytes, int height,
                                          intptr_t mx, intptr_t my, int width);
void ff_hevc_put_hevc_qpel_bi_h_10_neon(uint8_t *dstBytes,
                                         ptrdiff_t dstStrideBytes,
                                         const uint8_t *srcBytes,
                                         ptrdiff_t srcStrideBytes,
                                         const int16_t *src2, int height,
                                         intptr_t mx, intptr_t my, int width);
void ff_hevc_put_hevc_qpel_bi_v_10_neon(uint8_t *dstBytes,
                                         ptrdiff_t dstStrideBytes,
                                         const uint8_t *srcBytes,
                                         ptrdiff_t srcStrideBytes,
                                         const int16_t *src2, int height,
                                         intptr_t mx, intptr_t my, int width);
void ff_hevc_put_hevc_qpel_uni_hv_10_neon(uint8_t *dstBytes,
                                           ptrdiff_t dstStrideBytes,
                                           const uint8_t *srcBytes,
                                           ptrdiff_t srcStrideBytes, int height,
                                           intptr_t mx, intptr_t my, int width);
void ff_hevc_put_hevc_qpel_bi_hv_10_neon(uint8_t *dstBytes,
                                          ptrdiff_t dstStrideBytes,
                                          const uint8_t *srcBytes,
                                          ptrdiff_t srcStrideBytes,
                                          const int16_t *src2, int height,
                                          intptr_t mx, intptr_t my, int width);
void ff_hevc_put_hevc_epel_hv_10_neon(int16_t *dst, const uint8_t *srcBytes,
                                       ptrdiff_t srcStrideBytes, int height,
                                       intptr_t mx, intptr_t my, int width);
void ff_hevc_put_hevc_epel_uni_hv_10_neon(uint8_t *dstBytes,
                                           ptrdiff_t dstStrideBytes,
                                           const uint8_t *srcBytes,
                                           ptrdiff_t srcStrideBytes, int height,
                                           intptr_t mx, intptr_t my, int width);
void ff_hevc_put_hevc_epel_bi_hv_10_neon(uint8_t *dstBytes,
                                          ptrdiff_t dstStrideBytes,
                                          const uint8_t *srcBytes,
                                          ptrdiff_t srcStrideBytes,
                                          const int16_t *src2, int height,
                                          intptr_t mx, intptr_t my, int width);

static inline FilterAccumulator filter8_accumulate(const int16_t *src,
                                                    ptrdiff_t stride,
                                                    const int8_t *filter)
{
    int16x8_t pixels = vld1q_s16(src - 3 * stride);
    FilterAccumulator accumulator = {
        vmull_n_s16(vget_low_s16(pixels), filter[0]),
        vmull_n_s16(vget_high_s16(pixels), filter[0]),
    };

    for (int tap = 1; tap < 8; tap++) {
        pixels = vld1q_s16(src + (tap - 3) * stride);
        accumulator.low = vmlal_n_s16(accumulator.low,
                                      vget_low_s16(pixels), filter[tap]);
        accumulator.high = vmlal_n_s16(accumulator.high,
                                       vget_high_s16(pixels), filter[tap]);
    }
    return accumulator;
}

static inline int16x8_t filter8_shift2(const int16_t *src, ptrdiff_t stride,
                                       const int8_t *filter)
{
    FilterAccumulator accumulator = filter8_accumulate(src, stride, filter);

    return vcombine_s16(vshrn_n_s32(accumulator.low, 2),
                        vshrn_n_s32(accumulator.high, 2));
}

static inline int16x8_t filter8_shift6(const int16_t *src, ptrdiff_t stride,
                                       const int8_t *filter)
{
    FilterAccumulator accumulator = filter8_accumulate(src, stride, filter);

    return vcombine_s16(vshrn_n_s32(accumulator.low, 6),
                        vshrn_n_s32(accumulator.high, 6));
}

static inline int16x4_t filter8_shift2_4(const int16_t *src,
                                         ptrdiff_t stride,
                                         const int8_t *filter)
{
    int16x4_t pixels = vld1_s16(src - 3 * stride);
    int32x4_t accumulator = vmull_n_s16(pixels, filter[0]);

    for (int tap = 1; tap < 8; tap++) {
        pixels = vld1_s16(src + (tap - 3) * stride);
        accumulator = vmlal_n_s16(accumulator, pixels, filter[tap]);
    }
    return vshrn_n_s32(accumulator, 2);
}

static inline int16x4_t filter8_shift6_4(const int16_t *src,
                                         ptrdiff_t stride,
                                         const int8_t *filter)
{
    int16x4_t pixels = vld1_s16(src - 3 * stride);
    int32x4_t accumulator = vmull_n_s16(pixels, filter[0]);

    for (int tap = 1; tap < 8; tap++) {
        pixels = vld1_s16(src + (tap - 3) * stride);
        accumulator = vmlal_n_s16(accumulator, pixels, filter[tap]);
    }
    return vshrn_n_s32(accumulator, 6);
}

static inline void filter8_shift6_2rows(const int16_t *src,
                                         ptrdiff_t stride,
                                         const int8_t *filter,
                                         int16x8_t *first,
                                         int16x8_t *second)
{
    int16x8_t current = vld1q_s16(src - 3 * stride);
    int16x8_t next = vld1q_s16(src - 2 * stride);
    FilterAccumulator firstAccumulator = {
        vmull_n_s16(vget_low_s16(current), filter[0]),
        vmull_n_s16(vget_high_s16(current), filter[0]),
    };
    FilterAccumulator secondAccumulator = {
        vmull_n_s16(vget_low_s16(next), filter[0]),
        vmull_n_s16(vget_high_s16(next), filter[0]),
    };

    current = next;
    for (int tap = 1; tap < 8; tap++) {
        next = vld1q_s16(src + (tap - 2) * stride);
        firstAccumulator.low = vmlal_n_s16(firstAccumulator.low,
                                            vget_low_s16(current), filter[tap]);
        firstAccumulator.high = vmlal_n_s16(firstAccumulator.high,
                                             vget_high_s16(current), filter[tap]);
        secondAccumulator.low = vmlal_n_s16(secondAccumulator.low,
                                             vget_low_s16(next), filter[tap]);
        secondAccumulator.high = vmlal_n_s16(secondAccumulator.high,
                                              vget_high_s16(next), filter[tap]);
        current = next;
    }
    *first = vcombine_s16(vshrn_n_s32(firstAccumulator.low, 6),
                           vshrn_n_s32(firstAccumulator.high, 6));
    *second = vcombine_s16(vshrn_n_s32(secondAccumulator.low, 6),
                            vshrn_n_s32(secondAccumulator.high, 6));
}

static inline void filter8_shift6_2rows_4(const int16_t *src,
                                           ptrdiff_t stride,
                                           const int8_t *filter,
                                           int16x4_t *first,
                                           int16x4_t *second)
{
    int16x4_t current = vld1_s16(src - 3 * stride);
    int16x4_t next = vld1_s16(src - 2 * stride);
    int32x4_t firstAccumulator = vmull_n_s16(current, filter[0]);
    int32x4_t secondAccumulator = vmull_n_s16(next, filter[0]);

    current = next;
    for (int tap = 1; tap < 8; tap++) {
        next = vld1_s16(src + (tap - 2) * stride);
        firstAccumulator = vmlal_n_s16(firstAccumulator, current, filter[tap]);
        secondAccumulator = vmlal_n_s16(secondAccumulator, next, filter[tap]);
        current = next;
    }
    *first = vshrn_n_s32(firstAccumulator, 6);
    *second = vshrn_n_s32(secondAccumulator, 6);
}

static inline int filter_scalar(const int16_t *src, ptrdiff_t stride,
                                const int8_t *filter)
{
    int sum = 0;

    for (int tap = 0; tap < 8; tap++) {
        sum += filter[tap] * src[(tap - 3) * stride];
    }
    return sum;
}

static inline FilterAccumulator filter4_accumulate(const int16_t *src,
                                                    ptrdiff_t stride,
                                                    const int8_t *filter)
{
    int16x8_t pixels = vld1q_s16(src - stride);
    FilterAccumulator accumulator = {
        vmull_n_s16(vget_low_s16(pixels), filter[0]),
        vmull_n_s16(vget_high_s16(pixels), filter[0]),
    };

    for (int tap = 1; tap < 4; tap++) {
        pixels = vld1q_s16(src + (tap - 1) * stride);
        accumulator.low = vmlal_n_s16(accumulator.low,
                                      vget_low_s16(pixels), filter[tap]);
        accumulator.high = vmlal_n_s16(accumulator.high,
                                       vget_high_s16(pixels), filter[tap]);
    }
    return accumulator;
}

static inline int16x8_t filter4_shift2(const int16_t *src, ptrdiff_t stride,
                                       const int8_t *filter)
{
    FilterAccumulator accumulator = filter4_accumulate(src, stride, filter);

    return vcombine_s16(vshrn_n_s32(accumulator.low, 2),
                        vshrn_n_s32(accumulator.high, 2));
}

static inline int16x8_t filter4_shift6(const int16_t *src, ptrdiff_t stride,
                                       const int8_t *filter)
{
    FilterAccumulator accumulator = filter4_accumulate(src, stride, filter);

    return vcombine_s16(vshrn_n_s32(accumulator.low, 6),
                        vshrn_n_s32(accumulator.high, 6));
}

static inline int16x4_t filter4_shift2_4(const int16_t *src,
                                         ptrdiff_t stride,
                                         const int8_t *filter)
{
    int16x4_t pixels = vld1_s16(src - stride);
    int32x4_t accumulator = vmull_n_s16(pixels, filter[0]);

    for (int tap = 1; tap < 4; tap++) {
        pixels = vld1_s16(src + (tap - 1) * stride);
        accumulator = vmlal_n_s16(accumulator, pixels, filter[tap]);
    }
    return vshrn_n_s32(accumulator, 2);
}

static inline int16x4_t filter4_shift6_4(const int16_t *src,
                                         ptrdiff_t stride,
                                         const int8_t *filter)
{
    int16x4_t pixels = vld1_s16(src - stride);
    int32x4_t accumulator = vmull_n_s16(pixels, filter[0]);

    for (int tap = 1; tap < 4; tap++) {
        pixels = vld1_s16(src + (tap - 1) * stride);
        accumulator = vmlal_n_s16(accumulator, pixels, filter[tap]);
    }
    return vshrn_n_s32(accumulator, 6);
}

static inline int filter4_scalar(const int16_t *src, ptrdiff_t stride,
                                 const int8_t *filter)
{
    int sum = 0;

    for (int tap = 0; tap < 4; tap++) {
        sum += filter[tap] * src[(tap - 1) * stride];
    }
    return sum;
}

static void filter_horizontal(int16_t *tmp, const uint16_t *src,
                              ptrdiff_t srcStride, int rows, int width,
                              int tmpStride, const int8_t *filter)
{
    ff_hevc_qpel_10_filter_horizontal_neon(tmp, src, srcStride, rows,
                                           width, tmpStride, filter);
}

static void filter4_horizontal(int16_t *tmp, const uint16_t *src,
                               ptrdiff_t srcStride, int rows, int width,
                               int tmpStride, const int8_t *filter)
{
    for (int y = 0; y < rows; y++) {
        int x = 0;

        for (; x + 8 <= width; x += 8) {
            vst1q_s16(tmp + x, filter4_shift2((const int16_t *)src + x, 1,
                                               filter));
        }
        for (; x + 4 <= width; x += 4) {
            vst1_s16(tmp + x, filter4_shift2_4((const int16_t *)src + x, 1,
                                               filter));
        }
        for (; x < width; x++) {
            tmp[x] = filter4_scalar((const int16_t *)src + x, 1,
                                    filter) >> 2;
        }
        src += srcStride;
        tmp += tmpStride;
    }
}

static inline uint16x8_t clip_shift(int16x8_t value, int offset, int shift)
{
    int32x4_t low = vmovl_s16(vget_low_s16(value));
    int32x4_t high = vmovl_s16(vget_high_s16(value));
    const int32x4_t zero = vdupq_n_s32(0);
    const int32x4_t maximum = vdupq_n_s32(1023);

    low = vshlq_s32(vaddq_s32(low, vdupq_n_s32(offset)),
                    vdupq_n_s32(-shift));
    high = vshlq_s32(vaddq_s32(high, vdupq_n_s32(offset)),
                     vdupq_n_s32(-shift));
    low = vminq_s32(vmaxq_s32(low, zero), maximum);
    high = vminq_s32(vmaxq_s32(high, zero), maximum);
    return vcombine_u16(vqmovun_s32(low), vqmovun_s32(high));
}

static inline uint16x8_t clip_shift_sum(int16x8_t first, int16x8_t second,
                                        int offset, int shift)
{
    int32x4_t low = vaddl_s16(vget_low_s16(first), vget_low_s16(second));
    int32x4_t high = vaddl_s16(vget_high_s16(first), vget_high_s16(second));
    const int32x4_t zero = vdupq_n_s32(0);
    const int32x4_t maximum = vdupq_n_s32(1023);

    low = vshlq_s32(vaddq_s32(low, vdupq_n_s32(offset)),
                    vdupq_n_s32(-shift));
    high = vshlq_s32(vaddq_s32(high, vdupq_n_s32(offset)),
                     vdupq_n_s32(-shift));
    low = vminq_s32(vmaxq_s32(low, zero), maximum);
    high = vminq_s32(vmaxq_s32(high, zero), maximum);
    return vcombine_u16(vqmovun_s32(low), vqmovun_s32(high));
}

static inline uint16x4_t clip_shift_4(int16x4_t value, int offset, int shift)
{
    int32x4_t wide = vmovl_s16(value);

    wide = vshlq_s32(vaddq_s32(wide, vdupq_n_s32(offset)),
                     vdupq_n_s32(-shift));
    wide = vminq_s32(vmaxq_s32(wide, vdupq_n_s32(0)),
                     vdupq_n_s32(1023));
    return vqmovun_s32(wide);
}

static inline uint16x4_t clip_shift_sum_4(int16x4_t first, int16x4_t second,
                                          int offset, int shift)
{
    int32x4_t wide = vaddl_s16(first, second);

    wide = vshlq_s32(vaddq_s32(wide, vdupq_n_s32(offset)),
                     vdupq_n_s32(-shift));
    wide = vminq_s32(vmaxq_s32(wide, vdupq_n_s32(0)),
                     vdupq_n_s32(1023));
    return vqmovun_s32(wide);
}

static void qpel_1d(int16_t *dst, const uint16_t *src,
                    ptrdiff_t srcStride, int height, int width,
                    const int8_t *filter, ptrdiff_t filterStride)
{
    for (int y = 0; y < height; y++) {
        int x = 0;

        for (; x + 8 <= width; x += 8) {
            vst1q_s16(dst + x, filter8_shift2((const int16_t *)src + x,
                                              filterStride, filter));
        }
        for (; x + 4 <= width; x += 4) {
            vst1_s16(dst + x, filter8_shift2_4((const int16_t *)src + x,
                                              filterStride, filter));
        }
        for (; x < width; x++) {
            dst[x] = filter_scalar((const int16_t *)src + x,
                                   filterStride, filter) >> 2;
        }
        src += srcStride;
        dst += MAX_PB_SIZE;
    }
}

static void qpel_uni_1d(uint16_t *dst, ptrdiff_t dstStride,
                        const uint16_t *src, ptrdiff_t srcStride,
                        int height, int width, const int8_t *filter,
                        ptrdiff_t filterStride)
{
    for (int y = 0; y < height; y++) {
        int x = 0;

        for (; x + 8 <= width; x += 8) {
            int16x8_t value = filter8_shift2((const int16_t *)src + x,
                                              filterStride, filter);
            vst1q_u16(dst + x, clip_shift(value, 8, 4));
        }
        for (; x + 4 <= width; x += 4) {
            int16x4_t value = filter8_shift2_4((const int16_t *)src + x,
                                               filterStride, filter);

            vst1_u16(dst + x, clip_shift_4(value, 8, 4));
        }
        for (; x < width; x++) {
            int value = filter_scalar((const int16_t *)src + x,
                                      filterStride, filter) >> 2;
            dst[x] = av_clip_uintp2((value + 8) >> 4, 10);
        }
        src += srcStride;
        dst += dstStride;
    }
}

static void qpel_bi_1d(uint16_t *dst, ptrdiff_t dstStride,
                       const uint16_t *src, ptrdiff_t srcStride,
                       const int16_t *src2, int height, int width,
                       const int8_t *filter, ptrdiff_t filterStride)
{
    for (int y = 0; y < height; y++) {
        int x = 0;

        for (; x + 8 <= width; x += 8) {
            int16x8_t value = filter8_shift2((const int16_t *)src + x,
                                              filterStride, filter);
            vst1q_u16(dst + x, clip_shift_sum(value, vld1q_s16(src2 + x),
                                              16, 5));
        }
        for (; x + 4 <= width; x += 4) {
            int16x4_t value = filter8_shift2_4((const int16_t *)src + x,
                                               filterStride, filter);

            vst1_u16(dst + x,
                     clip_shift_sum_4(value, vld1_s16(src2 + x), 16, 5));
        }
        for (; x < width; x++) {
            int value = filter_scalar((const int16_t *)src + x,
                                      filterStride, filter) >> 2;
            dst[x] = av_clip_uintp2((value + src2[x] + 16) >> 5, 10);
        }
        src += srcStride;
        src2 += MAX_PB_SIZE;
        dst += dstStride;
    }
}

void ff_hevc_put_hevc_qpel_h_10_neon(int16_t *dst, const uint8_t *srcBytes,
                                      ptrdiff_t srcStrideBytes, int height,
                                      intptr_t mx, intptr_t my, int width)
{
    const uint16_t *src = (const uint16_t *)srcBytes;
    const ptrdiff_t srcStride = srcStrideBytes / sizeof(*src);

    qpel_1d(dst, src, srcStride, height, width, ff_hevc_qpel_filters[mx], 1);
}

void ff_hevc_put_hevc_qpel_v_10_neon(int16_t *dst, const uint8_t *srcBytes,
                                      ptrdiff_t srcStrideBytes, int height,
                                      intptr_t mx, intptr_t my, int width)
{
    const uint16_t *src = (const uint16_t *)srcBytes;
    const ptrdiff_t srcStride = srcStrideBytes / sizeof(*src);

    qpel_1d(dst, src, srcStride, height, width, ff_hevc_qpel_filters[my],
            srcStride);
}

void ff_hevc_put_hevc_qpel_uni_h_10_neon(uint8_t *dstBytes,
                                          ptrdiff_t dstStrideBytes,
                                          const uint8_t *srcBytes,
                                          ptrdiff_t srcStrideBytes, int height,
                                          intptr_t mx, intptr_t my, int width)
{
    const uint16_t *src = (const uint16_t *)srcBytes;
    uint16_t *dst = (uint16_t *)dstBytes;
    const ptrdiff_t srcStride = srcStrideBytes / sizeof(*src);
    const ptrdiff_t dstStride = dstStrideBytes / sizeof(*dst);

    qpel_uni_1d(dst, dstStride, src, srcStride, height, width,
                 ff_hevc_qpel_filters[mx], 1);
}

void ff_hevc_put_hevc_qpel_uni_v_10_neon(uint8_t *dstBytes,
                                          ptrdiff_t dstStrideBytes,
                                          const uint8_t *srcBytes,
                                          ptrdiff_t srcStrideBytes, int height,
                                          intptr_t mx, intptr_t my, int width)
{
    const uint16_t *src = (const uint16_t *)srcBytes;
    uint16_t *dst = (uint16_t *)dstBytes;
    const ptrdiff_t srcStride = srcStrideBytes / sizeof(*src);
    const ptrdiff_t dstStride = dstStrideBytes / sizeof(*dst);

    qpel_uni_1d(dst, dstStride, src, srcStride, height, width,
                 ff_hevc_qpel_filters[my], srcStride);
}

void ff_hevc_put_hevc_qpel_bi_h_10_neon(uint8_t *dstBytes,
                                         ptrdiff_t dstStrideBytes,
                                         const uint8_t *srcBytes,
                                         ptrdiff_t srcStrideBytes,
                                         const int16_t *src2, int height,
                                         intptr_t mx, intptr_t my, int width)
{
    const uint16_t *src = (const uint16_t *)srcBytes;
    uint16_t *dst = (uint16_t *)dstBytes;
    const ptrdiff_t srcStride = srcStrideBytes / sizeof(*src);
    const ptrdiff_t dstStride = dstStrideBytes / sizeof(*dst);

    qpel_bi_1d(dst, dstStride, src, srcStride, src2, height, width,
                ff_hevc_qpel_filters[mx], 1);
}

void ff_hevc_put_hevc_qpel_bi_v_10_neon(uint8_t *dstBytes,
                                         ptrdiff_t dstStrideBytes,
                                         const uint8_t *srcBytes,
                                         ptrdiff_t srcStrideBytes,
                                         const int16_t *src2, int height,
                                         intptr_t mx, intptr_t my, int width)
{
    const uint16_t *src = (const uint16_t *)srcBytes;
    uint16_t *dst = (uint16_t *)dstBytes;
    const ptrdiff_t srcStride = srcStrideBytes / sizeof(*src);
    const ptrdiff_t dstStride = dstStrideBytes / sizeof(*dst);

    qpel_bi_1d(dst, dstStride, src, srcStride, src2, height, width,
                ff_hevc_qpel_filters[my], srcStride);
}

void ff_hevc_put_hevc_qpel_hv_10_neon(int16_t *dst, const uint8_t *srcBytes,
                                       ptrdiff_t srcStrideBytes, int height,
                                       intptr_t mx, intptr_t my, int width)
{
    DECLARE_ALIGNED(16, int16_t, tmp)[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];
    const uint16_t *src = (const uint16_t *)srcBytes;
    const ptrdiff_t srcStride = srcStrideBytes / sizeof(*src);
    const int8_t *horizontalFilter = ff_hevc_qpel_filters[mx];
    const int8_t *verticalFilter = ff_hevc_qpel_filters[my];
    const int tmpStride = MAX_PB_SIZE;
    const int16_t *verticalSrc;

    src -= QPEL_EXTRA_BEFORE * srcStride;
    filter_horizontal(tmp, src, srcStride, height + QPEL_EXTRA, width,
                      tmpStride, horizontalFilter);

    verticalSrc = tmp + QPEL_EXTRA_BEFORE * tmpStride;
    int y = 0;

    for (; y + 1 < height; y += 2) {
        int x = 0;

        for (; x + 8 <= width; x += 8) {
            int16x8_t first;
            int16x8_t second;

            filter8_shift6_2rows(verticalSrc + x, tmpStride, verticalFilter,
                                  &first, &second);
            vst1q_s16(dst + x, first);
            vst1q_s16(dst + MAX_PB_SIZE + x, second);
        }
        for (; x + 4 <= width; x += 4) {
            int16x4_t first;
            int16x4_t second;

            filter8_shift6_2rows_4(verticalSrc + x, tmpStride, verticalFilter,
                                    &first, &second);
            vst1_s16(dst + x, first);
            vst1_s16(dst + MAX_PB_SIZE + x, second);
        }
        for (; x < width; x++) {
            dst[x] = filter_scalar(verticalSrc + x, tmpStride,
                                   verticalFilter) >> 6;
            dst[MAX_PB_SIZE + x] =
                filter_scalar(verticalSrc + tmpStride + x, tmpStride,
                              verticalFilter) >> 6;
        }
        verticalSrc += 2 * tmpStride;
        dst += 2 * MAX_PB_SIZE;
    }
    for (; y < height; y++) {
        int x = 0;

        for (; x + 8 <= width; x += 8) {
            vst1q_s16(dst + x, filter8_shift6(verticalSrc + x,
                                              tmpStride, verticalFilter));
        }
        for (; x + 4 <= width; x += 4) {
            vst1_s16(dst + x, filter8_shift6_4(verticalSrc + x,
                                              tmpStride, verticalFilter));
        }
        for (; x < width; x++) {
            dst[x] = filter_scalar(verticalSrc + x, tmpStride,
                                   verticalFilter) >> 6;
        }
        verticalSrc += tmpStride;
        dst += MAX_PB_SIZE;
    }
}

void ff_hevc_put_hevc_qpel_uni_hv_10_neon(uint8_t *dstBytes,
                                           ptrdiff_t dstStrideBytes,
                                           const uint8_t *srcBytes,
                                           ptrdiff_t srcStrideBytes, int height,
                                           intptr_t mx, intptr_t my, int width)
{
    DECLARE_ALIGNED(16, int16_t, tmp)[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];
    const uint16_t *src = (const uint16_t *)srcBytes;
    uint16_t *dst = (uint16_t *)dstBytes;
    const ptrdiff_t srcStride = srcStrideBytes / sizeof(*src);
    const ptrdiff_t dstStride = dstStrideBytes / sizeof(*dst);
    const int8_t *horizontalFilter = ff_hevc_qpel_filters[mx];
    const int8_t *verticalFilter = ff_hevc_qpel_filters[my];
    const int tmpStride = MAX_PB_SIZE;
    const int16_t *verticalSrc;

    src -= QPEL_EXTRA_BEFORE * srcStride;
    filter_horizontal(tmp, src, srcStride, height + QPEL_EXTRA, width,
                      tmpStride, horizontalFilter);

    verticalSrc = tmp + QPEL_EXTRA_BEFORE * tmpStride;
    int y = 0;

    for (; y + 1 < height; y += 2) {
        int x = 0;

        for (; x + 8 <= width; x += 8) {
            int16x8_t first;
            int16x8_t second;

            filter8_shift6_2rows(verticalSrc + x, tmpStride, verticalFilter,
                                  &first, &second);
            vst1q_u16(dst + x, clip_shift(first, 8, 4));
            vst1q_u16(dst + dstStride + x, clip_shift(second, 8, 4));
        }
        for (; x + 4 <= width; x += 4) {
            int16x4_t first;
            int16x4_t second;

            filter8_shift6_2rows_4(verticalSrc + x, tmpStride, verticalFilter,
                                    &first, &second);
            vst1_u16(dst + x, clip_shift_4(first, 8, 4));
            vst1_u16(dst + dstStride + x, clip_shift_4(second, 8, 4));
        }
        for (; x < width; x++) {
            int first = filter_scalar(verticalSrc + x, tmpStride,
                                      verticalFilter) >> 6;
            int second = filter_scalar(verticalSrc + tmpStride + x, tmpStride,
                                       verticalFilter) >> 6;

            dst[x] = av_clip_uintp2((first + 8) >> 4, 10);
            dst[dstStride + x] = av_clip_uintp2((second + 8) >> 4, 10);
        }
        verticalSrc += 2 * tmpStride;
        dst += 2 * dstStride;
    }
    for (; y < height; y++) {
        int x = 0;

        for (; x + 8 <= width; x += 8) {
            int16x8_t value = filter8_shift6(verticalSrc + x,
                                              tmpStride, verticalFilter);
            vst1q_u16(dst + x, clip_shift(value, 8, 4));
        }
        for (; x + 4 <= width; x += 4) {
            int16x4_t value = filter8_shift6_4(verticalSrc + x,
                                               tmpStride, verticalFilter);

            vst1_u16(dst + x, clip_shift_4(value, 8, 4));
        }
        for (; x < width; x++) {
            int value = filter_scalar(verticalSrc + x, tmpStride,
                                      verticalFilter) >> 6;
            dst[x] = av_clip_uintp2((value + 8) >> 4, 10);
        }
        verticalSrc += tmpStride;
        dst += dstStride;
    }
}

void ff_hevc_put_hevc_qpel_bi_hv_10_neon(uint8_t *dstBytes,
                                          ptrdiff_t dstStrideBytes,
                                          const uint8_t *srcBytes,
                                          ptrdiff_t srcStrideBytes,
                                          const int16_t *src2, int height,
                                          intptr_t mx, intptr_t my, int width)
{
    DECLARE_ALIGNED(16, int16_t, tmp)[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];
    const uint16_t *src = (const uint16_t *)srcBytes;
    uint16_t *dst = (uint16_t *)dstBytes;
    const ptrdiff_t srcStride = srcStrideBytes / sizeof(*src);
    const ptrdiff_t dstStride = dstStrideBytes / sizeof(*dst);
    const int8_t *horizontalFilter = ff_hevc_qpel_filters[mx];
    const int8_t *verticalFilter = ff_hevc_qpel_filters[my];
    const int tmpStride = MAX_PB_SIZE;
    const int16_t *verticalSrc;

    src -= QPEL_EXTRA_BEFORE * srcStride;
    filter_horizontal(tmp, src, srcStride, height + QPEL_EXTRA, width,
                      tmpStride, horizontalFilter);

    verticalSrc = tmp + QPEL_EXTRA_BEFORE * tmpStride;
    int y = 0;

    for (; y + 1 < height; y += 2) {
        int x = 0;

        for (; x + 8 <= width; x += 8) {
            int16x8_t first;
            int16x8_t second;

            filter8_shift6_2rows(verticalSrc + x, tmpStride, verticalFilter,
                                  &first, &second);
            vst1q_u16(dst + x,
                      clip_shift_sum(first, vld1q_s16(src2 + x), 16, 5));
            vst1q_u16(dst + dstStride + x,
                      clip_shift_sum(second,
                                     vld1q_s16(src2 + MAX_PB_SIZE + x),
                                     16, 5));
        }
        for (; x + 4 <= width; x += 4) {
            int16x4_t first;
            int16x4_t second;

            filter8_shift6_2rows_4(verticalSrc + x, tmpStride, verticalFilter,
                                    &first, &second);
            vst1_u16(dst + x,
                     clip_shift_sum_4(first, vld1_s16(src2 + x), 16, 5));
            vst1_u16(dst + dstStride + x,
                     clip_shift_sum_4(second,
                                      vld1_s16(src2 + MAX_PB_SIZE + x),
                                      16, 5));
        }
        for (; x < width; x++) {
            int first = filter_scalar(verticalSrc + x, tmpStride,
                                      verticalFilter) >> 6;
            int second = filter_scalar(verticalSrc + tmpStride + x, tmpStride,
                                       verticalFilter) >> 6;

            dst[x] = av_clip_uintp2((first + src2[x] + 16) >> 5, 10);
            dst[dstStride + x] =
                av_clip_uintp2((second + src2[MAX_PB_SIZE + x] + 16) >> 5,
                               10);
        }
        verticalSrc += 2 * tmpStride;
        src2 += 2 * MAX_PB_SIZE;
        dst += 2 * dstStride;
    }
    for (; y < height; y++) {
        int x = 0;

        for (; x + 8 <= width; x += 8) {
            int16x8_t value = filter8_shift6(verticalSrc + x,
                                              tmpStride, verticalFilter);
            vst1q_u16(dst + x, clip_shift_sum(value, vld1q_s16(src2 + x),
                                              16, 5));
        }
        for (; x + 4 <= width; x += 4) {
            int16x4_t value = filter8_shift6_4(verticalSrc + x,
                                               tmpStride, verticalFilter);

            vst1_u16(dst + x,
                     clip_shift_sum_4(value, vld1_s16(src2 + x), 16, 5));
        }
        for (; x < width; x++) {
            int value = filter_scalar(verticalSrc + x, tmpStride,
                                      verticalFilter) >> 6;
            dst[x] = av_clip_uintp2((value + src2[x] + 16) >> 5, 10);
        }
        verticalSrc += tmpStride;
        src2 += MAX_PB_SIZE;
        dst += dstStride;
    }
}

void ff_hevc_put_hevc_epel_hv_10_neon(int16_t *dst, const uint8_t *srcBytes,
                                       ptrdiff_t srcStrideBytes, int height,
                                       intptr_t mx, intptr_t my, int width)
{
    DECLARE_ALIGNED(16, int16_t, tmp)[(MAX_PB_SIZE + EPEL_EXTRA) * MAX_PB_SIZE];
    const uint16_t *src = (const uint16_t *)srcBytes;
    const ptrdiff_t srcStride = srcStrideBytes / sizeof(*src);
    const int8_t *horizontalFilter = ff_hevc_epel_filters[mx];
    const int8_t *verticalFilter = ff_hevc_epel_filters[my];
    const int tmpStride = MAX_PB_SIZE;
    const int16_t *verticalSrc;

    src -= EPEL_EXTRA_BEFORE * srcStride;
    filter4_horizontal(tmp, src, srcStride, height + EPEL_EXTRA, width,
                       tmpStride, horizontalFilter);

    verticalSrc = tmp + EPEL_EXTRA_BEFORE * tmpStride;
    for (int y = 0; y < height; y++) {
        int x = 0;

        for (; x + 8 <= width; x += 8) {
            vst1q_s16(dst + x, filter4_shift6(verticalSrc + x,
                                               tmpStride, verticalFilter));
        }
        for (; x + 4 <= width; x += 4) {
            vst1_s16(dst + x, filter4_shift6_4(verticalSrc + x,
                                               tmpStride, verticalFilter));
        }
        for (; x < width; x++) {
            dst[x] = filter4_scalar(verticalSrc + x, tmpStride,
                                    verticalFilter) >> 6;
        }
        verticalSrc += tmpStride;
        dst += MAX_PB_SIZE;
    }
}

void ff_hevc_put_hevc_epel_uni_hv_10_neon(uint8_t *dstBytes,
                                           ptrdiff_t dstStrideBytes,
                                           const uint8_t *srcBytes,
                                           ptrdiff_t srcStrideBytes, int height,
                                           intptr_t mx, intptr_t my, int width)
{
    DECLARE_ALIGNED(16, int16_t, tmp)[(MAX_PB_SIZE + EPEL_EXTRA) * MAX_PB_SIZE];
    const uint16_t *src = (const uint16_t *)srcBytes;
    uint16_t *dst = (uint16_t *)dstBytes;
    const ptrdiff_t srcStride = srcStrideBytes / sizeof(*src);
    const ptrdiff_t dstStride = dstStrideBytes / sizeof(*dst);
    const int8_t *horizontalFilter = ff_hevc_epel_filters[mx];
    const int8_t *verticalFilter = ff_hevc_epel_filters[my];
    const int tmpStride = MAX_PB_SIZE;
    const int16_t *verticalSrc;

    src -= EPEL_EXTRA_BEFORE * srcStride;
    filter4_horizontal(tmp, src, srcStride, height + EPEL_EXTRA, width,
                       tmpStride, horizontalFilter);

    verticalSrc = tmp + EPEL_EXTRA_BEFORE * tmpStride;
    for (int y = 0; y < height; y++) {
        int x = 0;

        for (; x + 8 <= width; x += 8) {
            int16x8_t value = filter4_shift6(verticalSrc + x,
                                              tmpStride, verticalFilter);
            vst1q_u16(dst + x, clip_shift(value, 8, 4));
        }
        for (; x + 4 <= width; x += 4) {
            int16x4_t value = filter4_shift6_4(verticalSrc + x,
                                               tmpStride, verticalFilter);

            vst1_u16(dst + x, clip_shift_4(value, 8, 4));
        }
        for (; x < width; x++) {
            int value = filter4_scalar(verticalSrc + x, tmpStride,
                                       verticalFilter) >> 6;
            dst[x] = av_clip_uintp2((value + 8) >> 4, 10);
        }
        verticalSrc += tmpStride;
        dst += dstStride;
    }
}

void ff_hevc_put_hevc_epel_bi_hv_10_neon(uint8_t *dstBytes,
                                          ptrdiff_t dstStrideBytes,
                                          const uint8_t *srcBytes,
                                          ptrdiff_t srcStrideBytes,
                                          const int16_t *src2, int height,
                                          intptr_t mx, intptr_t my, int width)
{
    DECLARE_ALIGNED(16, int16_t, tmp)[(MAX_PB_SIZE + EPEL_EXTRA) * MAX_PB_SIZE];
    const uint16_t *src = (const uint16_t *)srcBytes;
    uint16_t *dst = (uint16_t *)dstBytes;
    const ptrdiff_t srcStride = srcStrideBytes / sizeof(*src);
    const ptrdiff_t dstStride = dstStrideBytes / sizeof(*dst);
    const int8_t *horizontalFilter = ff_hevc_epel_filters[mx];
    const int8_t *verticalFilter = ff_hevc_epel_filters[my];
    const int tmpStride = MAX_PB_SIZE;
    const int16_t *verticalSrc;

    src -= EPEL_EXTRA_BEFORE * srcStride;
    filter4_horizontal(tmp, src, srcStride, height + EPEL_EXTRA, width,
                       tmpStride, horizontalFilter);

    verticalSrc = tmp + EPEL_EXTRA_BEFORE * tmpStride;
    for (int y = 0; y < height; y++) {
        int x = 0;

        for (; x + 8 <= width; x += 8) {
            int16x8_t value = filter4_shift6(verticalSrc + x,
                                              tmpStride, verticalFilter);
            vst1q_u16(dst + x, clip_shift_sum(value, vld1q_s16(src2 + x),
                                              16, 5));
        }
        for (; x + 4 <= width; x += 4) {
            int16x4_t value = filter4_shift6_4(verticalSrc + x,
                                               tmpStride, verticalFilter);

            vst1_u16(dst + x,
                     clip_shift_sum_4(value, vld1_s16(src2 + x), 16, 5));
        }
        for (; x < width; x++) {
            int value = filter4_scalar(verticalSrc + x, tmpStride,
                                       verticalFilter) >> 6;
            dst[x] = av_clip_uintp2((value + src2[x] + 16) >> 5, 10);
        }
        verticalSrc += tmpStride;
        src2 += MAX_PB_SIZE;
        dst += dstStride;
    }
}
