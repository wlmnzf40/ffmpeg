# 优化点 13：AArch64 HEVC CABAC 归一化精简

- 提交：`c8ce289`（`avcodec/aarch64: streamline HEVC CABAC renormalization`）
- 补丁：[`0013-avcodec-aarch64-streamline-HEVC-CABAC-renormalizatio.patch`](../../../patches/hevc-720p-aarch64/0013-avcodec-aarch64-streamline-HEVC-CABAC-renormalizatio.patch)
- 前置：优化点 12

## 优化背景

`ff_hevc_hls_residual_coding` 是剩余最大的解码热点，内部高频调用 CABAC。每个
bin 原来从 `ff_h264_norm_shift` 查归一化移位，并在不需要 refill 的常见路径上
也提前计算表地址。

## 代码走读

`libavcodec/hevc/cabac.c` 的 HEVC `GET_CABAC` 直接选择
`get_cabac_inline`，避免热点调用退化为非内联包装。

`libavcodec/aarch64/cabac.h` 用两条无数据访问指令计算相同结果：

```text
shift = clz(range) - 23
```

CABAC 此处 `range` 的规范范围保证该式与原归一化表完全一致。原本每个 bin 都
执行的 norm 表基址相加被移到 `low` 的低 16 位为零、确实需要 refill 的分支，
减少常见路径指令。状态转移表读取、range/low 更新和字节流边界逻辑均未改变。

## 验证与性能

完整 15000 帧阶段测试从 32.261 s 降至 32.033 s：先替换 CLZ 后约 32.081 s，
再延迟 refill 地址计算后约 32.033 s。3000 帧 RGB24 哈希与基线一致。

该优化作用于 AArch64 CABAC 通用热点，不依赖当前视频的某个运动矢量或特定
分辨率；非 AArch64 继续使用原实现。
