# 优化点 14：稀疏 HEVC IDCT 列组跳过

- 提交：`9c18d11`（`avcodec/aarch64: skip empty HEVC IDCT column groups`）
- 补丁：[`0014-avcodec-aarch64-skip-empty-HEVC-IDCT-column-groups.patch`](../../../patches/hevc-720p-aarch64/0014-avcodec-aarch64-skip-empty-HEVC-IDCT-column-groups.patch)
- 前置：优化点 13

## 优化背景

HEVC 熵解码把最后一个非零系数列通过 `col_limit` 传给 IDCT。16×16 和 32×32
AArch64 NEON 原实现仍无条件执行全部第一遍列变换。实际压缩块常只有低频列
非零，剩余列组的完整矩阵运算最终只会得到零。

## 代码走读

`hevcdsp_idct_neon.S` 在进入函数时保存 `col_limit`。每个第一遍内核处理连续
4 列；当 `col_limit <= 4*i + 3` 时，该组不存在需要变换的非零列，因此：

1. 不调用 `func_tr_16x4_firstpass` 或 `func_tr_32x4_firstpass`；
2. 用成对 128-bit store 把对应临时转置块清零；
3. 第二遍保持不变，仍从完整、已定义的临时缓冲区读取。

条件按 FFmpeg 当前 `col_limit` 的“有效列数上界”语义设置，覆盖汇编宏生成的
8-bit 和 10-bit 16×16/32×32 IDCT。8×8 的同类候选在鲲鹏 920 上实测变慢，
因此已经回退，不包含在补丁中。

## 验证与性能

完整 15000 帧从 32.033 s 降至 30.927 s，约降低 3.45%；对应 user 为
433.526 s，sys 为 4.362 s。perf 中 16×16/32×32 第一遍 IDCT 占比明显下降。

3000 帧 RGB24 输出与基线的 SHA-256 均为
`9bf7a192699267ddfb6615887231209758dcdc695aa740c6ca75cb5f4709f792`。跳过的输入
数学上全为零，输出是 bit-exact；非稀疏列组仍执行原汇编。
