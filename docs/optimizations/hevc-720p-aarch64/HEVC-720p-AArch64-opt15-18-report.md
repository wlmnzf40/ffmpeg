# HEVC-720p Main10 鲲鹏 920 优化续篇（patch15～18）

## 结论

patch15～18 叠加在已有 BoostKit 和 patch1、2、5～14 之后，不应用线程调节
patch3，也不依赖 PGO patch4。目标输入和 15000 帧 benchmark 均未改变。

| 构建与阶段 | real | user | 说明 |
|---|---:|---:|---|
| 历史原始对照 | 43.327 s | - | 同一输入与命令 |
| patch14 后代表值 | 30.927 s | - | 上一轮正式版本 |
| patch15 RGB 精确算术 | 31.077 s | 394.485 s | real 受波动影响，user 明显下降 |
| patch16～17 组合区间 | 约 30.7 s | - | hscale 初始化分派与 CABAC 状态缓存 |
| patch18，GCC 静态 FFmpeg 库 | 30.516 s | 388.966 s | 三组成对周期中位数约 -1.47% |
| patch18，Clang 17 LTO | **29.563 s** | 378.658 s | 当前保留的最好完整构建 |

相对 43.327 s，当前代表值降低约 31.8%，吞吐约 1.47×。尚未达到约 20 s / 2×，
因此不作 2×声明。纯解码、CABAC 串行段和插值仍是主要下限；在不增加线程的
约束下，剩余 9 秒不能通过一个编译开关补齐。

## Benchmark 与正确性

```bash
ffmpeg -nostdin -stats -hide_banner -threads 0 \
    -i /data/wanglimin/HEVC-720p-10min.mov \
    -frames:v 15000 -an -sn -vf format=rgb24 -f null -
```

前 300 帧 `framemd5` 管道 SHA-256：

```text
903d4493e85b12ed85ee76a4683065ef602c2546f19a1fa807ad8c8ad7f60543
```

输入自身的 non-monotonically-increasing DTS 提示不代表解码失败。墙钟比较看
`real`，总 CPU 工作量同时记录 `user`；短测必须 A/B 交替并结合 cycles 和
instructions，避免把频率与温度波动误认为优化。

## 本轮否决项

- CABAC MLPS 表基址提升：指令约 -0.39%，周期中位数约 +0.3%；
- significance state 指针提升：指令约 -0.24%，周期中位数仅约 -0.1%；
- RGB24 transpose 改用 `tbl`：指令约 -0.79%，周期中位数约 +1.27%；
- LTO 最终链接改为 `-mcpu=tsv110`：两组完整成对测试均慢 0.13～0.33 s；
- GCC LTO、qpel 特例、IDCT DC store、对称 epel 等此前候选均未稳定获益。

这些实验均已从正式代码撤回。

## 应用顺序

```bash
git am patches/hevc-720p-aarch64/0015-*.patch
git am patches/hevc-720p-aarch64/0016-*.patch
git am patches/hevc-720p-aarch64/0017-*.patch
git am patches/hevc-720p-aarch64/0018-*.patch
```

如从未打补丁的 FFmpeg 7.1.1 开始，先按旧报告依次应用 BoostKit、patch1、2、
5～14。不要应用 patch3；patch4 的 PGO 对当前输入没有稳定收益。
