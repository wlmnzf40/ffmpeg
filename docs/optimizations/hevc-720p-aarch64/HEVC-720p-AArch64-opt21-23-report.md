# HEVC-720p 鲲鹏 920 代码优化续篇：有效补丁 21～23

## 结论

在已提交的 patch20 基线上，仅叠加三个 HEVC CABAC 代码优化，完整
15000 帧 benchmark 的 `real` 平均从 30.599 秒降到 28.684 秒，
约改善 6.26%。测试始终使用 FFmpeg 原有 `-threads 0`；没有采用线程上限
调整的旧 patch3，也没有将 PGO 或短测波动计入收益。

| 交叉顺序 | patch20 `real` | patch21～23 `real` |
|---|---:|---:|
| patch20 → patch21～23 | 30.652 s | 28.623 s |
| patch21～23 → patch20 | 30.546 s | 28.745 s |

距离约 20 秒目标仍差约 8.7 秒，不能宣称已经实现两倍加速。

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
相同 GCC 静态构建配置；完整命令为：

```bash
ffmpeg -nostdin -v fatal -threads 0 \
    -i /data/wanglimin/HEVC-720p-10min.mov \
    -frames:v 15000 -an -sn -vf format=rgb24 -f null -
```

两版完整 15000 帧 RGB24 `framemd5` 管道 SHA-256 均为：
`ae62ca4ee03f9f4e7c5fa6997b82a9b5881ae318ff19f16881b16b65596630c2`。
`git diff --check` 通过。仓库根目录没有 `codestyle` 文件，因此没有可运行的
该项扫描器。

本轮曾尝试 QPEL 双累加链、4 像素/对称分支、EPEL 对称快路径、
CABAC 循环展开、DC 缓冲区免清零、greater-than-one 位掩码和符号位无分支取反；
单独隔离后或无稳定完整 `real` 收益，或变慢，故未纳入本组有效补丁。

## 边界与下一步

当前视频上的正确性和性能已经覆盖 15000 帧；其他 HEVC 视频和不同编译器
仍需分别复测，不能把上述数字直接外推。纯解码采样显示剩余热点主要在
残差 CABAC、水平 QPEL/EPEL 插值及反变换，继续接近 20 秒需要更大的
解码算法或向量化改动，而不是调整线程数。
