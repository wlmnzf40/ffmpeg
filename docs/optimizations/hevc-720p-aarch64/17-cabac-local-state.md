# 优化点 17：HEVC CABAC low/range 寄存器缓存

- 提交：`41c1cda2c`（`avcodec/aarch64: keep HEVC CABAC state in registers`）
- 补丁：[`0017-avcodec-aarch64-keep-HEVC-CABAC-state-in-registers.patch`](../../../patches/hevc-720p-aarch64/0017-avcodec-aarch64-keep-HEVC-CABAC-state-in-registers.patch)
- 前置：优化点 16

## 优化背景

残差解码会在短循环中连续解出多个 CABAC bin。原 AArch64 内联函数每次都通过
`CABACContext` 读写 `low` 和 `range`，即使下一次调用紧邻当前调用，也会形成
重复的结构体访存和编译器别名约束。

## 代码走读

`libavcodec/aarch64/cabac.h` 增加 `get_cabac_local_aarch64`：调用者传入
`lowValue` 和 `rangeValue`，汇编主体仍使用原状态转移表、range/low 更新、refill
和字节流边界逻辑。既有 `get_cabac_inline_aarch64` 继续保留，单次调用行为不变。

`libavcodec/hevc/cabac.c` 在两个高频区域使用局部状态：

1. X/Y 最后非零系数前缀循环开始时各读取一次状态，两个循环结束后统一写回；
2. 一个 4×4 系数组的 significant-coeff 循环开始时读取，循环结束后写回。

非 AArch64 或没有内联汇编时继续编译原 `GET_CABAC` 路径。仅供回退路径使用的
helper 也用条件编译包围，避免 Clang 的 unused-function 警告。

## 验证与性能

输出哈希与优化点 15、16 一致。该点与初始化分派组合后把 GCC 完整测试推进到
约 30.7 s 区间；单次 real 的差异受频率波动影响较大，因此不声明一个脱离成对
测试的百分比。它降低的是通用 HEVC 残差 CABAC 循环访存，不依赖当前视频的
分辨率或某一个 scan 模式。
