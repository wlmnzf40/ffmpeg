/*
 * AArch64 NEON 8x8 transpose
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "libavutil/aarch64/asm.S"

function ff_transpose_8x8_16_neon, export=1
        ld1             {v0.8h}, [x0], x1
        ld1             {v1.8h}, [x0], x1
        ld1             {v2.8h}, [x0], x1
        ld1             {v3.8h}, [x0], x1
        ld1             {v4.8h}, [x0], x1
        ld1             {v5.8h}, [x0], x1
        ld1             {v6.8h}, [x0], x1
        ld1             {v7.8h}, [x0]

        trn1            v16.8h, v0.8h, v1.8h
        trn2            v17.8h, v0.8h, v1.8h
        trn1            v18.8h, v2.8h, v3.8h
        trn2            v19.8h, v2.8h, v3.8h
        trn1            v20.8h, v4.8h, v5.8h
        trn2            v21.8h, v4.8h, v5.8h
        trn1            v22.8h, v6.8h, v7.8h
        trn2            v23.8h, v6.8h, v7.8h

        trn1            v0.4s, v16.4s, v18.4s
        trn2            v1.4s, v16.4s, v18.4s
        trn1            v2.4s, v17.4s, v19.4s
        trn2            v3.4s, v17.4s, v19.4s
        trn1            v4.4s, v20.4s, v22.4s
        trn2            v5.4s, v20.4s, v22.4s
        trn1            v6.4s, v21.4s, v23.4s
        trn2            v7.4s, v21.4s, v23.4s

        trn1            v16.2d, v0.2d, v4.2d
        trn1            v17.2d, v2.2d, v6.2d
        trn1            v18.2d, v1.2d, v5.2d
        trn1            v19.2d, v3.2d, v7.2d
        trn2            v20.2d, v0.2d, v4.2d
        trn2            v21.2d, v2.2d, v6.2d
        trn2            v22.2d, v1.2d, v5.2d
        trn2            v23.2d, v3.2d, v7.2d

        st1             {v16.8h}, [x2], x3
        st1             {v17.8h}, [x2], x3
        st1             {v18.8h}, [x2], x3
        st1             {v19.8h}, [x2], x3
        st1             {v20.8h}, [x2], x3
        st1             {v21.8h}, [x2], x3
        st1             {v22.8h}, [x2], x3
        st1             {v23.8h}, [x2]
        ret
endfunc

.macro transpose8_u8 r0, r1, r2, r3, r4, r5, r6, r7
        trn1            v24.8b, \r0\().8b, \r1\().8b
        trn2            v25.8b, \r0\().8b, \r1\().8b
        trn1            v26.8b, \r2\().8b, \r3\().8b
        trn2            v27.8b, \r2\().8b, \r3\().8b
        trn1            v28.8b, \r4\().8b, \r5\().8b
        trn2            v29.8b, \r4\().8b, \r5\().8b
        trn1            v30.8b, \r6\().8b, \r7\().8b
        trn2            v31.8b, \r6\().8b, \r7\().8b

        trn1            \r0\().4h, v24.4h, v26.4h
        trn2            \r1\().4h, v24.4h, v26.4h
        trn1            \r2\().4h, v25.4h, v27.4h
        trn2            \r3\().4h, v25.4h, v27.4h
        trn1            \r4\().4h, v28.4h, v30.4h
        trn2            \r5\().4h, v28.4h, v30.4h
        trn1            \r6\().4h, v29.4h, v31.4h
        trn2            \r7\().4h, v29.4h, v31.4h

        trn1            v24.2s, \r0\().2s, \r4\().2s
        trn1            v25.2s, \r2\().2s, \r6\().2s
        trn1            v26.2s, \r1\().2s, \r5\().2s
        trn1            v27.2s, \r3\().2s, \r7\().2s
        trn2            v28.2s, \r0\().2s, \r4\().2s
        trn2            v29.2s, \r2\().2s, \r6\().2s
        trn2            v30.2s, \r1\().2s, \r5\().2s
        trn2            v31.2s, \r3\().2s, \r7\().2s

        mov             \r0\().8b, v24.8b
        mov             \r1\().8b, v25.8b
        mov             \r2\().8b, v26.8b
        mov             \r3\().8b, v27.8b
        mov             \r4\().8b, v28.8b
        mov             \r5\().8b, v29.8b
        mov             \r6\().8b, v30.8b
        mov             \r7\().8b, v31.8b
.endm

function ff_transpose_8x8_24_neon, export=1
        ld3             {v0.8b, v1.8b, v2.8b}, [x0], x1
        ld3             {v3.8b, v4.8b, v5.8b}, [x0], x1
        ld3             {v6.8b, v7.8b, v8.8b}, [x0], x1
        ld3             {v9.8b, v10.8b, v11.8b}, [x0], x1
        ld3             {v12.8b, v13.8b, v14.8b}, [x0], x1
        ld3             {v15.8b, v16.8b, v17.8b}, [x0], x1
        ld3             {v18.8b, v19.8b, v20.8b}, [x0], x1
        ld3             {v21.8b, v22.8b, v23.8b}, [x0]

        transpose8_u8   v0, v3, v6, v9, v12, v15, v18, v21
        transpose8_u8   v1, v4, v7, v10, v13, v16, v19, v22
        transpose8_u8   v2, v5, v8, v11, v14, v17, v20, v23

        st3             {v0.8b, v1.8b, v2.8b}, [x2], x3
        st3             {v3.8b, v4.8b, v5.8b}, [x2], x3
        st3             {v6.8b, v7.8b, v8.8b}, [x2], x3
        st3             {v9.8b, v10.8b, v11.8b}, [x2], x3
        st3             {v12.8b, v13.8b, v14.8b}, [x2], x3
        st3             {v15.8b, v16.8b, v17.8b}, [x2], x3
        st3             {v18.8b, v19.8b, v20.8b}, [x2], x3
        st3             {v21.8b, v22.8b, v23.8b}, [x2]
        ret
endfunc
