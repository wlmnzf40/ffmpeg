# 优化点 6：HEVC 边界强度按预测单元复用

- 提交：`d25fd99`（`avcodec/hevc: reuse boundary strength within prediction units`）
- 补丁：[`0006-avcodec-hevc-reuse-boundary-strength-within-predicti.patch`](../../../patches/hevc-720p-aarch64/0006-avcodec-hevc-reuse-boundary-strength-within-predicti.patch)
- 前置：FFmpeg 7.1.1；可直接叠加在 BoostKit + 优化点 1、2 上
- 明确不包含：自动线程数调节、PGO/LTO、跳过 deblock

## 热点与重复计算

在 BoostKit + 优化点 1、2、`-threads 0` 的受控基线上，perf 显示：

- `ff_hevc_deblocking_boundary_strengths`：18.04% cycles；
- `boundary_strength`：3.94% cycles；
- 两者合计约 22%。

边界强度表每 4 个像素保存一个值，而测试码流的最小预测单元为 8×8。同一个
预测单元内相邻两个 4 像素位置引用完全相同的 `MvField` 和参考帧列表，原循环
却会重复执行运动矢量、参考帧编号和双向预测关系判断。

## 代码走读

改动位于 `libavcodec/hevc/filter.c`：

1. `boundary_strength` 改为直接接收当前和相邻的 `RefPicList`，避免每次调用
   通过 `HEVCContext -> cur_frame -> refPicList` 重复追踪指针。
2. 双向预测路径先把四个实际参考帧编号读入局部变量，后续“同向、交叉、四者
   相同”判断复用这些值。
3. 外部水平边界用 `cachedXPu`，外部垂直边界用 `cachedYPu`。只有 PU 索引
   发生变化且 CBF 没有提前决定边界强度时，才重新计算运动边界强度。
4. 内部水平边界同样按 `x_pu` 复用结果；对于该码流，每两个 4 像素输出只
   计算一次 `boundary_strength`。
5. 内部垂直边界对常见的 `log2_min_pu_size == 3`（8×8 PU）一次计算后写入
   相邻两条 4 像素边界行。若最小 PU 不是 8，或坐标未按 8 对齐，则保留原始
   通用循环，避免改变其他合法码流的行为。
6. `cbf_luma`、水平/垂直边界数组、边界数组宽度和变换尺寸均在函数入口缓存；
   写入地址改为递增指针，减少热点循环中的结构体加载和地址乘加。

该优化只消除同一 PU 内的重复计算。所有参考帧关系、运动矢量阈值、CBF、帧内
预测及 slice/tile 边界判断均保持原语义，deblock 没有被关闭或跳过。

## 性能结果

环境为 Kunpeng 920 7270Z，输入为 1280×720 HEVC Main、15000 帧。基线只
包含 BoostKit + 优化点 1、2，双方均使用原始 `-threads 0`，没有 patch3 和
PGO。12 组前后交替运行的中位数：

| 指标 | 优化前 | 优化后 | 变化 |
|---|---:|---:|---:|
| real | 1.463 s | 1.364 s | 提升 6.82% |
| task-clock | 24693.5 ms | 22635.0 ms | 降低 8.34% |
| cycles | 71.273 G | 65.320 G | 降低 8.35% |
| instructions | 195.939 G | 173.225 G | 降低 11.59% |

优化后的 perf 中，边界强度两个符号合计约 15%，说明重复工作已经明显下降，
但参考列表加载和运动矢量比较仍有后续优化空间。

## 正确性与门禁

- 基线与优化版前 300 帧 RGB24 MD5 一致；
- 第 10000 帧 RGB24 MD5 均为 `5ea2d3b80b547605602422f667690b31`；
- 全部解码帧的原生 YUV420 `framecrc` 文本 SHA-256 均为
  `b32a73512313062dd115911a0adfba24280292baf0404e5e05012ee3f11d1449`；
- AArch64 完整编译和 15000 帧运行通过。

由于现有样本的最小 PU 为 8×8，8×8 快速分支还应在更多不同 SPS、tile 和
slice 组合的 HEVC 样本上持续回归；不满足快速条件的码流使用通用路径。
