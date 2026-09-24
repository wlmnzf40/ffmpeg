# HEVC-720p Main10 鲲鹏 920 优化续篇（patch9～14）

## 结论

本轮以已有 BoostKit + patch1/2/5/6/7/8 为代码基础，继续加入六个彼此独立的
优化提交。全程未应用线程调节 patch3，也未依赖 PGO patch4；benchmark 仍使用
FFmpeg 原始 `-threads 0`。

在 `/data/wanglimin/HEVC-720p-10min.mov` 上，原始基线记录为 43.327 s，当前
组合版为 30.927 s，墙钟降低 28.6%，吞吐约 1.40×。当前结果尚未达到用户要求
的约 20 s / 2×，因此报告不作 2×声明。

| 阶段 | 完整 15000 帧 real | 说明 |
|---|---:|---|
| 原始对照 | 43.327 s | 同一输入和命令 |
| 本轮开始时已有优化栈 | 约 33.265 s | BoostKit + 非线程优化 |
| Main10 垂直 qpel 双行复用 | 32.594 s | patch9 内部阶段 |
| RGB24 8-lane 垂直滤波 | 32.506 s | patch10 |
| qpel 半像素对称汇编 | 32.261 s | patch9 内部后续阶段 |
| CABAC CLZ + 冷分支地址延迟 | 32.033 s | patch13 |
| 16/32 稀疏 IDCT | 30.927 s | patch14，当前最好 |

恒等 hscale（patch11）和 transpose（patch12）主要降低 CPU 工作量，单独 real
收益落在共享机器噪声范围，未把最好单次值写成稳定提升。

## Benchmark

```bash
ffmpeg -nostdin -stats -hide_banner -threads 0 \
    -i /data/wanglimin/HEVC-720p-10min.mov \
    -frames:v 15000 -an -sn -vf format=rgb24 -f null -
```

应以相同机器、相同 CPU/NUMA 绑定、相同构建选项做 A/B 交替复测。`real` 是用户
感受到的完成时间，`user` 用于判断总 CPU 工作量；两者都记录，但本任务的 20 s
目标以 `real` 为准。输入时间戳不单调会产生约 1481 条 DTS 警告，不影响输出
哈希和计时结束。

## 正确性

- AArch64 静态构建通过；
- 完整 15000 帧 benchmark 通过；
- 前 300 帧 RGB24 `framemd5` 管道 SHA-256：
  `903d4493e85b12ed85ee76a4683065ef602c2546f19a1fa807ad8c8ad7f60543`；
- 3000 帧基线和组合版 SHA-256 均为
  `9bf7a192699267ddfb6615887231209758dcdc695aa740c6ca75cb5f4709f792`。

## 应用顺序

patch9～14 依次叠加在 patch1、2、5、6、7、8 后：

```bash
git am patches/hevc-720p-aarch64/0001-*.patch
git am patches/hevc-720p-aarch64/0002-*.patch
git am patches/hevc-720p-aarch64/0005-*.patch
git am patches/hevc-720p-aarch64/0006-*.patch
git am patches/hevc-720p-aarch64/0007-*.patch
git am patches/hevc-720p-aarch64/0008-*.patch
git am patches/hevc-720p-aarch64/0009-*.patch
git am patches/hevc-720p-aarch64/0010-*.patch
git am patches/hevc-720p-aarch64/0011-*.patch
git am patches/hevc-720p-aarch64/0012-*.patch
git am patches/hevc-720p-aarch64/0013-*.patch
git am patches/hevc-720p-aarch64/0014-*.patch
```

不要应用 patch3（线程上限变化）和 patch4（PGO/LTO），这样可保持本报告的对比
口径。逐项代码走读为本目录的 `09-main10-interpolation-neon.md` 至
`14-sparse-idct-neon.md`。
