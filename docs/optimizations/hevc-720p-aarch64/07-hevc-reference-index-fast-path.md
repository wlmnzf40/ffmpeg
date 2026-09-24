# 优化点 7：HEVC 相同参考索引快速路径

- 提交：`813f0c4`（`avcodec/hevc: fast-path matching reference indices`）
- 补丁：[`0007-avcodec-hevc-fast-path-matching-reference-indices.patch`](../../../patches/hevc-720p-aarch64/0007-avcodec-hevc-fast-path-matching-reference-indices.patch)
- 前置：优化点 6；不依赖线程调节或 PGO

## 优化背景

优化点 6 消除了同一预测单元内重复计算边界强度，但每次真正进入
`boundary_strength` 时，仍需通过两个 `RefPicList` 和 `ref_idx` 读取实际参考
帧 POC。优化后的 perf 中，边界强度相关符号仍约占 15% cycles，热点主要落在
四次参考列表间接加载和后续依赖判断上。

多数内部 PU 边界的两侧使用同一个参考列表，而且 L0/L1 的 `ref_idx` 直接
相等。这种情况下可以先用索引证明参考帧相同，无需读取相邻侧的参考列表项。

## 代码走读

改动位于 `libavcodec/hevc/filter.c` 的 `boundary_strength`：

1. 函数先判断当前侧和相邻侧是否使用同一个 `RefPicList`。
2. 对双向预测，如果 L0、L1 索引分别相等，只读取当前侧两个实际参考 POC。
3. 当 L0 和 L1 指向不同图片时，可以直接执行同向运动矢量比较，省去相邻侧
   两次参考列表读取以及三组参考关系判断。
4. 如果 L0 和 L1 实际指向同一图片，仍进入原逻辑，同时计算同向和交叉运动
   矢量差，保持 HEVC 对等参考图片场景的语义。
5. 对单向预测，只有参考列表、预测方向和索引都相同才进入快速路径；不同方向、
   不同索引以及跨 slice 参考列表全部回退到原 POC 比较。
6. `boundary_strength` 使用 `av_always_inline`。对照测试表明，将慢路径拆成独立
   函数虽然减少代码体积和 CPU 总工作量，但会拖慢 16 线程 benchmark 的关键
   路径；完整内联是唯一同时改善 real、task-clock、cycles 和 instructions 的
   版本。

快速路径只利用“同一数组中相同索引必然指向同一项”这一恒真条件。它没有假设
不同索引一定引用不同图片，因此参考列表包含重复项时仍由原逻辑处理。

## 单项性能结果

基线为 patch1+2+5+6，候选再增加 patch7。双方均为 `-threads 0`，无
patch3/PGO。15 组交替测试中位数：

| 指标 | patch1+2+5+6 | 再加 patch7 | 变化 |
|---|---:|---:|---:|
| real | 1.314 s | 1.292 s | 提升 1.64% |
| task-clock | 21866 ms | 21244 ms | 降低 2.84% |
| cycles | 63.074 G | 61.256 G | 降低 2.88% |
| instructions | 166.386 G | 159.422 G | 降低 4.19% |

## 最新累计结果

直接比较“BoostKit + patch1+2”和“再加 patch5+6+7”，15 组交替测试为：

| 指标 | patch1+2 基线 | patch1+2+5+6+7 | 变化 |
|---|---:|---:|---:|
| real | 1.559 s | 1.353 s | 提升 13.23% |
| task-clock | 25191 ms | 22043 ms | 降低 12.50% |
| cycles | 72.695 G | 63.578 G | 降低 12.54% |
| instructions | 195.779 G | 159.404 G | 降低 18.58% |

## 正确性与门禁

- AArch64 完整编译和 15000 帧 benchmark 通过；
- 全部解码帧的 YUV420 `framecrc` 文本 SHA-256 仍为
  `b32a73512313062dd115911a0adfba24280292baf0404e5e05012ee3f11d1449`；
- 不修改线程数，不关闭 deblock，不改变运动矢量阈值；
- 不满足严格快速条件时执行原始参考 POC 比较。
