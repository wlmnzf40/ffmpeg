# 优化点 18：按扫描方向预组合 significance context

- 提交：`d15a5ad61`（`avcodec/hevc: precompose significance contexts by scan`）
- 补丁：[`0018-avcodec-hevc-precompose-significance-contexts-by-sca.patch`](../../../patches/hevc-720p-aarch64/0018-avcodec-hevc-precompose-significance-contexts-by-sca.patch)
- 前置：优化点 17

## 优化背景

significant-coeff 热循环按 scan position `n` 递减，但原 context 表按二维
`(x, y)` 排列。每个 bin 都要先读取 `scan_x_off[n]`、`scan_y_off[n]`，再计算
`(y << 2) + x` 才能读取 context。扫描表在运行期间不变，可以提前完成这层映射。

## 代码走读

`ctx_idx_map` 扩展为 `SCAN_DIAG`、`SCAN_HORIZ`、`SCAN_VERT` 三套表，每套仍含
原来的五类 context：4×4、三个 `prev_sig` 状态和 transform-skip 默认状态。

进入系数组时根据 `scan_idx` 和 context 类别选定一次表基址，热循环随后直接执行
`ctx_idx_map_p[n]`。这只预组合索引，没有更改 CABAC context 值。真正写回非零
系数时仍使用原扫描坐标表计算目标位置，因此扫描语义和输出布局不变。

## 验证与性能

- 300 帧 RGB24 哈希保持
  `903d4493e85b12ed85ee76a4683065ef602c2546f19a1fa807ad8c8ad7f60543`；
- GCC 三组成对 3000 帧测试中赢 2 组，周期中位数约下降 1.47%，指令约下降
  0.39%；
- GCC 完整测试由 30.718 s 降到 30.516 s；
- Clang LTO 组合版完整测试代表值由 30.190 s 降到 29.563 s。

三种 HEVC 扫描方向均有独立表，不会出现只对当前视频常见 scan 命中的覆盖缺口。
