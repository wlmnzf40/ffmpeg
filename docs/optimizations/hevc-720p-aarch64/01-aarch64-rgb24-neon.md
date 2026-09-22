# 优化点 1：AArch64 YUV420P → RGB24 NEON

- 提交：`4fac91c`（`swscale/aarch64: add NEON yuv420p to RGB24`）
- 补丁：[`0001-swscale-aarch64-add-NEON-yuv420p-to-RGB24.patch`](../../../patches/hevc-720p-aarch64/0001-swscale-aarch64-add-NEON-yuv420p-to-RGB24.patch)

## 背景与调用路径

测试命令中的 `-vf format=rgb24` 会把 HEVC 解码得到的 `yuv420p` 帧交给
libswscale。FFmpeg 7.1.1 的 AArch64 后端只注册了 ARGB、RGBA、ABGR、BGRA
和 GBRP，RGB24 因此回退到 `libswscale/yuv2rgb.c` 的通用 C 实现。

调用关系如下：

```text
ffmpeg format filter
  -> sws_scale()
    -> ff_get_unscaled_swscale()
      -> ff_get_unscaled_swscale_aarch64()
        -> yuv420p_to_rgb24_neon_wrapper()
          -> ff_yuv420p_to_rgb24_neon()
```

## 代码走读

`libswscale/aarch64/swscale_unscaled.c` 增加 RGB24 wrapper 和分派：

- wrapper 从 `SwsContext` 取出 YUV→RGB 的四个定点系数、亮度偏移和亮度系数；
- 输入宽度必须是 16 的倍数，高度必须为偶数；
- `SWS_ACCURATE_RND` 打开时不走该快速路径；
- CPU 必须由 `have_neon()` 判定支持 NEON。

`libswscale/aarch64/yuv2rgb_neon.S` 的内层循环每次处理 16 个像素：

1. 读取 8 个 U 和 8 个 V 样本；YUV420 中每个色度样本对应两个水平像素。
2. 用 `ushll` 扩展到 16 bit，并减去 128 的色度中心值。
3. 用 `sqdmulh` 完成 V→R、U/V→G、U→B 的定点乘法。
4. 用 `zip1/zip2` 将 8 个色度结果复制成 16 个像素对应的低/高半区。
5. 读取 16 个 Y，计算并叠加亮度分量。
6. 用 `sqrshrun` 做带饱和、舍入的 16→8 bit 收窄。
7. 用两条 `st3` 按 RGBRGB… 形式写出 48 字节，避免 RGBA 中间缓冲。

## 正确性

RGB24 输出与 FFmpeg 原有 RGBA NEON 路径去掉 alpha 后逐字节一致：

```text
rgb24_vs_rgba_rgb_exact=True
bytes=2764800
```

相对通用 C 路径的差异来自 FFmpeg 既有 NEON 定点舍入方式：单通道最大差 3，
PSNR 46.931 dB；新 RGB24 路径没有引入超出现有 RGBA NEON 的额外误差。

## 性能影响

在 15000 帧 HEVC-720p 测试中，首版 NEON 改动得到：

| 版本 | 平均 real | 平均 user | 墙钟提升 |
|---|---:|---:|---:|
| 原 OpenMP C RGB24 | 2.099 s | 320 s 左右 | 1.00× |
| RGB24 NEON | 1.509 s | 23–24 s | 1.39× |

CPU 总耗时下降约 13×。此后流水线从色彩转换受限转为 HEVC 解码受限。
