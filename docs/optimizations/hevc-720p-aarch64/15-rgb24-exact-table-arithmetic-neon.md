# 优化点 15：RGB24 精确查表算术 NEON 化

- 提交：`f6d90b6d1`（`swscale/aarch64: vectorize exact RGB24 table arithmetic`）
- 补丁：[`0015-swscale-aarch64-vectorize-exact-RGB24-table-arithmet.patch`](../../../patches/hevc-720p-aarch64/0015-swscale-aarch64-vectorize-exact-RGB24-table-arithmet.patch)
- 前置：优化点 14

## 优化背景

优化点 10 已把亮度 2-tap 和色度 4-tap 垂直滤波向量化，但滤波后的 16 个像素
仍逐对执行 RGB 查表。每对像素需要读取 R、G、B 三组间接表指针，热点中存在
大量标量地址计算、相关访存和 6-byte 小写入。

## 代码走读

`libswscale/yuv2rgb.c` 在建立原 RGB 表时，把同一套定点参数保存到
`SwsContext`。这不是另一套颜色公式，而是原表生成公式的系数和偏置。

`libswscale/aarch64/output_rgb_neon.c` 的新路径一次处理 16 个像素：

1. `ComputeChromaOffset` 按原表的裁剪、乘法、移位和偏置顺序计算 U/V 偏移；
2. `ConvertChannel` 将共享色度扩展到相邻两个亮度像素，并与四组亮度基值相加；
3. `vqmovun_s16` 保持原来的无符号饱和语义；
4. `vst3q_u8` 一次交错写出 16 个 RGB24 像素。

因此热点不再做逐像素表指针追踪，但舍入点、裁剪范围和最终字节均与原查表
实现一致。其它像素格式和未命中的滤波形状仍走原路径。

## 验证与性能

前 300 帧 RGB24 `framemd5` 管道 SHA-256 为
`903d4493e85b12ed85ee76a4683065ef602c2546f19a1fa807ad8c8ad7f60543`。

完整测试的代表值由 31.102 s / 449.896 user s 变为
31.077 s / 394.485 user s。real 处于共享机器波动范围，但总 CPU 时间减少约
12.3%，说明被移除的间接查表确实是有效工作量，而不是计时偶然值。
