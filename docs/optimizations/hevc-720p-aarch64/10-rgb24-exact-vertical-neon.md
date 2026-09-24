# 优化点 10：RGB24 精确垂直滤波 NEON

- 提交：`4740ddf`（`swscale/aarch64: vectorize exact RGB24 vertical filtering`）
- 补丁：[`0010-swscale-aarch64-vectorize-exact-RGB24-vertical-filte.patch`](../../../patches/hevc-720p-aarch64/0010-swscale-aarch64-vectorize-exact-RGB24-vertical-filte.patch)
- 前置：优化点 9

## 优化背景

目标命令的 `format=rgb24` 需要 swscale 对亮度和色度中间行做垂直卷积。通用
路径逐像素累加滤波器，而目标视频最常见的是 2-tap/4-tap。这部分不能用近似
YUV 转换替代，否则 RGB 输出会发生舍入差异。

## 代码走读

`libswscale/aarch64/output_rgb_neon.c` 增加 `ff_yuv2rgb24_X_neon`：

1. `filter_vertical_2_8` 和 `filter_vertical_4_8` 一次处理 8 个亮度样本；
2. 使用 32-bit widening multiply-accumulate，完全保留原滤波系数、偏置、移位
   和饱和顺序；
3. 色度按 RGB24 输出要求共享 U/V 结果，再调用既有颜色矩阵和打包逻辑；
4. 4-lane 和标量尾部处理非 8 对齐宽度。

`libswscale/aarch64/swscale.c` 只在 `RGB24 + 16-bit 中间格式 + 支持的滤波形状`
同时满足时安装该函数。未命中时仍走原 `yuv2packedX`，因此换分辨率或滤波器
不会出现功能缺口。

## 验证与性能

8-lane 版本在完整测试的相邻阶段使 real 从 32.594 s 降至 32.506 s，user 从
438.644 s 降至 435.860 s。墙钟收益较小，但 CPU 工作量稳定下降。

RGB24 `framemd5` 与基线逐帧一致；没有采用曾测试过、但会改变像素结果的直接
YUV→RGB 近似算法。
