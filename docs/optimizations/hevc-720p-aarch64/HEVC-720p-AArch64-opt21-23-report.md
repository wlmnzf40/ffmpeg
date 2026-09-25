# HEVC-720p 鲲鹏 920 代码优化续篇：有效补丁 21～23

## 结论

2026-09-26 更正：原报告中的二进制混入未提交的 QPEL 等试验代码，且
`-threads 0` 会触发仓库早期 patch3 对自动 HEVC 帧线程数的修改；原先的
30.599 → 28.684 秒不能作为“已提交代码、线程数不变”的可复现结论。

现在从 Git 提交 `d6925b63a`（patch20）和 `8943fd041`（patch21～23）
分别全新导出源码、使用相同 GCC 静态配置编译。在两个版本都显式固定
`-threads 16` 时，进程均为 52 个线程，完整 15000 帧 `real` 平均
从 30.360 秒降到 28.608 秒，改善 1.752 秒（5.77%）。这里没有把
自动线程数变化、PGO 或未提交试验代码计为收益。

| 交叉顺序 | patch20 `real` | patch21～23 `real` |
|---|---:|---:|
| patch20 → patch21～23 | 30.392 s | 28.602 s |
| patch21～23 → patch20 | 30.327 s | 28.613 s |

距离约 20 秒代码优化目标仍差约 8.6 秒，不能宣称已经实现两倍加速。
已提交版本以原命令 `-threads 0` 跑出过 20.789 秒，但该运行实际有
68 个进程线程，而固定 16 个解码线程的运行有 52 个；前者是自动线程
策略的效果，不计入本报告代码优化收益。

## 三个独立优化点

| 补丁 | 原理 | 独立走读 |
|---|---|---|
| 21 | 显著系数索引无分支写入 | [21-hevc-significance-branchless.md](21-hevc-significance-branchless.md) |
| 22 | 连续 CABAC 旁路位批量解码，异常情况回退逐位路径 | [22-hevc-cabac-bypass-batching.md](22-hevc-cabac-bypass-batching.md) |
| 23 | 反量化系数直接饱和裁剪到 16-bit 范围 | [23-hevc-coefficient-clamp.md](23-hevc-coefficient-clamp.md) |

三个补丁分别位于 `patches/hevc-720p-aarch64/0021-*.patch`、
`0022-*.patch`、`0023-*.patch`，按编号顺序应用于 patch20 后的源码。
在全新 patch20 工作树中依次 `git am` 三个补丁成功。

## 复现与正确性

测试输入：`/data/wanglimin/HEVC-720p-10min.mov`。同一台鲲鹏 920、
相同 GCC 静态构建配置；为排除仓库早期自动线程补丁的影响，两版都用
相同的 16 个解码线程。完整代码优化对照命令为：

```bash
ffmpeg -nostdin -v fatal -threads 16 \
    -i /data/wanglimin/HEVC-720p-10min.mov \
    -frames:v 15000 -an -sn -vf format=rgb24 -f null -
```

两版干净构建的完整 15000 帧 RGB24 `framemd5` 管道 SHA-256 均为：
`ae62ca4ee03f9f4e7c5fa6997b82a9b5881ae318ff19f16881b16b65596630c2`。
`git diff --check` 通过。仓库根目录没有 `codestyle` 文件，因此没有可运行的
该项扫描器。

单点复测（同样从对应提交取源码并固定 `-threads 16`）显示：patch21
平均改善约 0.724 秒；patch22 单独仅约 0.044 秒，无法证明稳定收益；
patch23 在 patch22 之后平均改善约 0.666 秒。不过，移除 patch22 后
保留 patch21 与 patch23 的版本，完整交叉测试平均为 29.596 秒，
而完整 patch21～23 为 28.745 秒，显示组合差异约 0.851 秒。
因此 patch22 不应单独宣称有稳定收益，但这段视频上的组合仍有收益。

本轮另试过 QPEL 七抽头零系数路径、EPEL 系数预加载、按实际非零列
缩小 IDCT 范围以及去量化比例预计算；没有稳定的完整 `real` 收益，
均未提交。

## 边界与下一步

当前视频上的正确性和性能已经覆盖 15000 帧；其他 HEVC 视频和不同编译器
仍需分别复测，不能把上述数字直接外推。纯解码采样显示剩余热点主要在
残差 CABAC、水平 QPEL/EPEL 插值及反变换，继续接近 20 秒需要更大的
解码算法或向量化改动，而不是调整线程数。
