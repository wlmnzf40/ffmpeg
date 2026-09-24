# HEVC-720p 鲲鹏 920B 有效优化续篇（不含线程调节与 PGO）

## 结论

客户不计入自动线程上限调整（原 patch3），且 PGO（原 patch4）在客户环境没有
稳定收益。本轮因此以“BoostKit + 原 patch1 + patch2”为唯一基线，双方都使用
FFmpeg 原始 `-threads 0`，不启用 PGO/LTO。

新增四个彼此独立的优化点：

1. RGB24 双行 NEON 内核消除色度寄存器往返复制，并合并为 128-bit `st3`；
2. HEVC deblock 边界强度在同一预测单元内复用；
3. 相同参考列表和参考索引使用严格快速路径，减少 POC 间接加载。
4. 整数像素双向预测直接做舍入平均，消除 16-bit 临时块。

继续优化后的最新 15 组基线/组合版交替测试中位数如下：

| 指标 | BoostKit + patch1/2 | 再加优化点 5/6/7/8 | 变化 |
|---|---:|---:|---:|
| real | 1.455 s | 1.292 s | 降低 11.20% |
| task-clock | 24465 ms | 21056 ms | 降低 13.94% |
| cycles | 70.598 G | 60.711 G | 降低 14.01% |
| instructions | 195.793 G | 150.045 G | 降低 23.37% |

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
- 优化点 7 走读：[`07-hevc-reference-index-fast-path.md`](07-hevc-reference-index-fast-path.md)
- 优化点 8 走读：[`08-hevc-direct-integer-bipred.md`](08-hevc-direct-integer-bipred.md)
- 后续未合入候选及量化依据：
  [`09-rejected-follow-up-candidates.md`](09-rejected-follow-up-candidates.md)
- 对应补丁位于 `patches/hevc-720p-aarch64/` 的 `0005`、`0006`、`0007`、`0008`。

四个新补丁按 0005～0008 的顺序叠加在 patch1/2 上；应用时可以跳过原 patch3
和 patch4。
