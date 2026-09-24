# 优化点 12：AArch64 8×8 转置 NEON

- 提交：`de27744`（`avfilter/aarch64: add NEON transpose kernels`）
- 补丁：[`0012-avfilter-aarch64-add-NEON-transpose-kernels.patch`](../../../patches/hevc-720p-aarch64/0012-avfilter-aarch64-add-NEON-transpose-kernels.patch)
- 前置：优化点 11

## 优化背景

目标 MOV 带 `rotate=-90` 元数据，FFmpeg 自动插入 transpose。完成 Main10 解码
和 RGB24 转换后，最终像素步长为 3，原路径逐像素复制 8×8 块。

## 代码走读

`vf_transpose_init_aarch64.c` 根据 `pixelStep` 安装三类 8×8 内核：

- 1-byte：NEON intrinsic 完成字节级 8×8 转置；
- 2-byte：汇编按 halfword、word、doubleword 三级 `trn1/trn2` 重排；
- 3-byte RGB24：八次 `ld3` 拆出 R/G/B，各通道分别做字节转置，再以八次
  `st3` 交织写回。

`vf_transpose.c` 保留原来的 C vtable 初始化，然后在 AArch64/NEON 上按像素
步长覆盖 8×8 函数。边缘不足 8×8 的区域仍由原 C 路径处理。该设计覆盖常见
灰度、16-bit 和 RGB24 视频，不绑定当前输入。

## 验证与性能

3000 帧阶段测试的 user 约从 92.6 s 降至 91.4～92.5 s；real 在噪声范围内。
转置不是剩余主热点，因此该补丁保留为小幅 CPU 优化，不宣称显著墙钟收益。
3000 帧 RGB24 哈希与未启用该内核的基线一致。
