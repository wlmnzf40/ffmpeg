# 优化点 20：隔离 GCC significance CABAC 热循环

- 提交：`edbd9fc7a`（`avcodec/hevc: isolate GCC significance CABAC loop`）
- 补丁：[`0020-avcodec-hevc-isolate-GCC-significance-CABAC-loop.patch`](../../../patches/hevc-720p-aarch64/0020-avcodec-hevc-isolate-GCC-significance-CABAC-loop.patch)
- 前置：优化点 19

## 优化背景

`ff_hevc_hls_residual_coding` 是一个局部变量和分支很多的大函数。GCC 把
significant-coeff 的 CABAC 循环内联进去后，循环中出现多处栈 reload；它们并非
CABAC 算法要求，而是大函数寄存器压力造成的代码生成副作用。

## 代码走读

`DecodeSignificantCoeffFlagsLocal` 接收 CABAC、已选好的 context 表和输出索引：

1. 每个 4×4 系数组只调用一次 helper，而不是每个 bin 调用一次；
2. helper 内继续把 `low` 和 `range` 保存在局部寄存器中；
3. 循环结束后统一写回 CABAC 状态并返回新的非零系数计数；
4. 调用者比较调用前后的计数，保持原 `implicit_non_zero_coeff` 语义。

helper 的现场很小，使 GCC 能减少大函数中的 spill/reload。Clang 原本已能很好地
分配内联循环，因此用编译器条件保留原路径；非 AArch64 也继续使用原实现。

## 验证与性能

- GCC、Clang 的 300 帧哈希均为
  `903d4493e85b12ed85ee76a4683065ef602c2546f19a1fa807ad8c8ad7f60543`；
- 6 组成对 3000 帧测试中指令数稳定下降约 0.65%；
- 周期 3/6 获胜，中位数约下降 0.53%，属于小幅但有指令证据的收益；
- 两组完整 A/B 各赢一组，候选平均 30.381 s、patch19 对照平均 30.482 s。

因此该点不宣称稳定的单次墙钟百分比，价值主要是降低 GCC 总工作量。Clang LTO
继续使用原内联路径，当前 29.563 s 代表值不变。
