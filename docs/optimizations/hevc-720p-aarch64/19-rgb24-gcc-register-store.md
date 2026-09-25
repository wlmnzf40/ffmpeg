# 优化点 19：消除 GCC RGB24 向量栈 spill

- 提交：`737768356`（`swscale/aarch64: avoid GCC RGB24 vector spills`）
- 补丁：[`0019-swscale-aarch64-avoid-GCC-RGB24-vector-spills.patch`](../../../patches/hevc-720p-aarch64/0019-swscale-aarch64-avoid-GCC-RGB24-vector-spills.patch)
- 前置：优化点 18

## 优化背景

优化点 15 使用 `uint8x16x3_t` 调用 `vst3q_u8`。Clang 会把三个结果保留在连续
向量寄存器并直接 `st3`，但 GCC 12 为该结构体生成三次栈写入，再用一次
`ld1 {v0-v2}` 回读，最后才执行 `st3`。这段 spill 位于每 16 个 RGB24 像素都
执行一次的循环内。

## 代码走读

GCC 路径的 `StoreRgb24Gcc` 用局部固定寄存器变量把 R、G、B 约束到 `v0`、
`v1`、`v2`，随后以内联汇编发出一条：

```asm
st3 {v0.16b-v2.16b}, [dest]
```

编译器最多只需插入寄存器 move，不再把三路结果往返栈。实测 GCC 生成代码的
栈帧由 112 bytes 缩到 64 bytes，循环中的三条 `str q` 和一条 `ld1` 消失。

Clang 原本已经生成理想代码，因此通过编译器条件继续使用 `vst3q_u8`，避免
强制固定寄存器反而破坏 Clang 的分配结果。像素计算和存储字节顺序均未改变。

## 验证与性能

- GCC 和 Clang 的 300 帧哈希均保持
  `903d4493e85b12ed85ee76a4683065ef602c2546f19a1fa807ad8c8ad7f60543`；
- GCC 三组成对 3000 帧测试全部获胜，周期分别约下降 1.05%、0.70%、2.61%，
  中位数约下降 1.05%；
- 指令数稳定下降约 0.27%；
- GCC 完整 15000 帧代表值由 30.516 s 改善到 30.384 s；
- Clang 反汇编仍为直接 `st3 {v27-v29}`，因此 Clang LTO 的 29.563 s 结果不变。
