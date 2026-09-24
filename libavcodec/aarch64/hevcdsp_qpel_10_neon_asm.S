/*
 * AArch64 NEON HEVC 10-bit horizontal quarter-pixel interpolation
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "libavutil/aarch64/asm.S"

.macro qpel_load_accumulate tap, offset
        ldur            q3, [x7, #\offset]
        smlal           v4.4s, v3.4h, v0.h[\tap]
        smlal2          v5.4s, v3.8h, v0.h[\tap]
.endm

.macro qpel_load_accumulate_low tap, offset
        ldur            d3, [x7, #\offset]
        smlal           v4.4s, v3.4h, v0.h[\tap]
.endm

.macro qpel_pair_accumulate tap, first, second
        ldur            q1, [x7, #\first]
        ldur            q2, [x7, #\second]
        add             v3.8h, v1.8h, v2.8h
        smlal           v4.4s, v3.4h, v0.h[\tap]
        smlal2          v5.4s, v3.8h, v0.h[\tap]
.endm

.macro qpel_pair_accumulate_low tap, first, second
        ldur            d1, [x7, #\first]
        ldur            d2, [x7, #\second]
        add             v3.4h, v1.4h, v2.4h
        smlal           v4.4s, v3.4h, v0.h[\tap]
.endm

function ff_hevc_qpel_10_filter_horizontal_neon, export=1
        ld1             {v0.8b}, [x6]
        sxtl            v0.8h, v0.8b
        lsl             x2, x2, #1
        lsl             x5, x5, #1
        ldrsb           w10, [x6]
        cmp             w10, #-1
        b.ne            1f
        ldrsb           w10, [x6, #7]
        cmp             w10, #-1
        b.eq            .Lqpel_symmetric
1:
        sub             x7, x1, #6
        mov             x8, x0
        mov             w9, w4
2:
        cmp             w9, #8
        b.lt            3f
        ldr             q1, [x7]
        smull           v4.4s, v1.4h, v0.h[0]
        smull2          v5.4s, v1.8h, v0.h[0]
        qpel_load_accumulate 1, 2
        qpel_load_accumulate 2, 4
        qpel_load_accumulate 3, 6
        qpel_load_accumulate 4, 8
        qpel_load_accumulate 5, 10
        qpel_load_accumulate 6, 12
        qpel_load_accumulate 7, 14
        shrn            v4.4h, v4.4s, #2
        shrn2           v4.8h, v5.4s, #2
        str             q4, [x8], #16
        add             x7, x7, #16
        sub             w9, w9, #8
        b               2b
3:
        cmp             w9, #4
        b.lt            4f
        ldr             d1, [x7]
        smull           v4.4s, v1.4h, v0.h[0]
        qpel_load_accumulate_low 1, 2
        qpel_load_accumulate_low 2, 4
        qpel_load_accumulate_low 3, 6
        qpel_load_accumulate_low 4, 8
        qpel_load_accumulate_low 5, 10
        qpel_load_accumulate_low 6, 12
        qpel_load_accumulate_low 7, 14
        shrn            v4.4h, v4.4s, #2
        str             d4, [x8], #8
        add             x7, x7, #8
        sub             w9, w9, #4
4:
        cbz             w9, 5f
        ldr             d1, [x7]
        smull           v4.4s, v1.4h, v0.h[0]
        qpel_load_accumulate_low 1, 2
        qpel_load_accumulate_low 2, 4
        qpel_load_accumulate_low 3, 6
        qpel_load_accumulate_low 4, 8
        qpel_load_accumulate_low 5, 10
        qpel_load_accumulate_low 6, 12
        qpel_load_accumulate_low 7, 14
        shrn            v4.4h, v4.4s, #2
        str             s4, [x8]
5:
        add             x1, x1, x2
        add             x0, x0, x5
        subs            w3, w3, #1
        b.gt            1b
        ret

.Lqpel_symmetric:
10:
        sub             x7, x1, #6
        mov             x8, x0
        mov             w9, w4
11:
        cmp             w9, #8
        b.lt            12f
        ldur            q1, [x7]
        ldur            q2, [x7, #14]
        add             v3.8h, v1.8h, v2.8h
        smull           v4.4s, v3.4h, v0.h[0]
        smull2          v5.4s, v3.8h, v0.h[0]
        qpel_pair_accumulate 1, 2, 12
        qpel_pair_accumulate 2, 4, 10
        qpel_pair_accumulate 3, 6, 8
        shrn            v4.4h, v4.4s, #2
        shrn2           v4.8h, v5.4s, #2
        str             q4, [x8], #16
        add             x7, x7, #16
        sub             w9, w9, #8
        b               11b
12:
        cmp             w9, #4
        b.lt            13f
        ldur            d1, [x7]
        ldur            d2, [x7, #14]
        add             v3.4h, v1.4h, v2.4h
        smull           v4.4s, v3.4h, v0.h[0]
        qpel_pair_accumulate_low 1, 2, 12
        qpel_pair_accumulate_low 2, 4, 10
        qpel_pair_accumulate_low 3, 6, 8
        shrn            v4.4h, v4.4s, #2
        str             d4, [x8], #8
        add             x7, x7, #8
        sub             w9, w9, #4
13:
        cbz             w9, 14f
        ldur            d1, [x7]
        ldur            d2, [x7, #14]
        add             v3.4h, v1.4h, v2.4h
        smull           v4.4s, v3.4h, v0.h[0]
        qpel_pair_accumulate_low 1, 2, 12
        qpel_pair_accumulate_low 2, 4, 10
        qpel_pair_accumulate_low 3, 6, 8
        shrn            v4.4h, v4.4s, #2
        str             s4, [x8]
14:
        add             x1, x1, x2
        add             x0, x0, x5
        subs            w3, w3, #1
        b.gt            10b
        ret
endfunc
