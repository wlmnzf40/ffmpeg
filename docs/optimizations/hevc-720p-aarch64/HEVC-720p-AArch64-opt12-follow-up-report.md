# HEVC-720p 鲲鹏 920B 有效优化续篇（不含线程调节与 PGO）

## 结论

客户不计入自动线程上限调整（原 patch3），且 PGO（原 patch4）在客户环境没有
稳定收益。本轮因此以“BoostKit + 原 patch1 + patch2”为唯一基线，双方都使用
FFmpeg 原始 `-threads 0`，不启用 PGO/LTO。

新增两个彼此独立的优化点：

1. RGB24 双行 NEON 内核消除色度寄存器往返复制，并合并为 128-bit `st3`；
2. HEVC deblock 边界强度在同一预测单元内复用。

15 组基线/组合版交替测试中位数如下：

| 指标 | BoostKit + patch1/2 | 再加优化点 5/6 | 变化 |
|---|---:|---:|---:|
| real | 1.490 s | 1.386 s | 提升 7.01% |
| task-clock | 24403 ms | 22272 ms | 降低 8.73% |
| cycles | 70.480 G | 64.235 G | 降低 8.86% |
| instructions | 195.781 G | 166.430 G | 降低 14.99% |

短 benchmark 的 real 会受共享服务器调度影响，因此结论同时要求 real、
task-clock、cycles 和 instructions 多项一致改善，不采用单次最好值。

## Benchmark 命令

```bash
LD_LIBRARY_PATH=<prefix>/lib <prefix>/bin/ffmpeg \
    -nostdin -v fatal -threads 0 \
    -i HEVC-720p-10min.MOV \
    -vf format=rgb24 -f null -
```

PMU 数据通过 `perf stat -e task-clock,cycles,instructions` 采集。测试输入为
1280×720、25 fps、15000 帧 HEVC Main，机器为 Kunpeng 920 7270Z。

## 正确性

- `fate-checkasm-sw_yuv2rgb` 通过；
- 前 300 帧和第 10000 帧 RGB24 MD5 与基线一致；
- 全量原生 YUV420 `framecrc` 清单的 SHA-256 与基线一致；
- 完整 AArch64 编译和 15000 帧 benchmark 通过。

## 交付物

- 优化点 5 走读：[`05-aarch64-rgb24-register-pack.md`](05-aarch64-rgb24-register-pack.md)
- 优化点 6 走读：[`06-hevc-boundary-strength-pu-reuse.md`](06-hevc-boundary-strength-pu-reuse.md)
- 对应补丁位于 `patches/hevc-720p-aarch64/` 的 `0005`、`0006`。

两个新补丁只依赖 patch1/2 的最终代码状态；应用时可以跳过原 patch3 和 patch4。
