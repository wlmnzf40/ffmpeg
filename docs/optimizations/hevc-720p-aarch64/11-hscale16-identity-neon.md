# 优化点 11：16-bit 恒等横向缩放快速路径

- 提交：`75e44bb`（`swscale/aarch64: fast-path identity 16-bit horizontal scaling`）
- 补丁：[`0011-swscale-aarch64-fast-path-identity-16-bit-horizontal.patch`](../../../patches/hevc-720p-aarch64/0011-swscale-aarch64-fast-path-identity-16-bit-horizontal.patch)
- 前置：优化点 10

## 优化背景

本 benchmark 没有改变分辨率，但 10-bit 输入进入 swscale 后仍构造四抽头横向
滤波器。内部滤波器实际是单位映射；继续执行四抽头乘加、读 `filterPos` 和
系数表属于纯开销。

## 代码走读

`is_hscale16_identity` 检查移位、滤波器长度，以及首、中、尾三个位置的映射和
系数。只有确认整行是 FFmpeg 为同宽转换生成的恒等四抽头布局时才进入快速
路径。尾像素在四抽头表中的有效系数位于最后一项，检查逻辑对此单独处理。

`hscale16to15_identity_neon` 每次加载 8 个 16-bit 样本并左移 5 位，直接生成
原滤波器的 15-bit 中间表示；不足 8 个像素的尾部用标量处理。未通过严格检查
时继续调用 `ff_hscale16to15_4_neon_asm`。

## 覆盖范围与性能

该快速路径覆盖所有满足相同恒等滤波条件的宽度，不依赖 1280×720；真正缩放、
其他移位或不同滤波布局自动回退。

3000 帧阶段测试中，user 从约 109～111 s 降至约 91.5～97.4 s；real 约 6.95 s，
受解码并行和机器调度影响没有可确认的孤立墙钟改善。该补丁的价值主要是减少
CPU 消耗，未把波动当作墙钟收益。
