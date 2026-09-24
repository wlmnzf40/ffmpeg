# 优化点 9：Main10 HEVC 插值 NEON

- 提交：`e973000`（`avcodec/aarch64: optimize Main10 HEVC interpolation`）
- 补丁：[`0009-avcodec-aarch64-optimize-Main10-HEVC-interpolation.patch`](../../../patches/hevc-720p-aarch64/0009-avcodec-aarch64-optimize-Main10-HEVC-interpolation.patch)
- 前置：patch1、2、5～8；不依赖线程调节和 PGO

## 优化背景

目标视频是 HEVC Main10，解码后的运动补偿大量进入 10-bit qpel/epel 路径。
原 AArch64 初始化没有覆盖这组热点，因而仍执行通用 C。该补丁为水平、垂直、
二维，以及 uni/bi 输出组合补齐 Main10 NEON 实现。

## 代码走读

`libavcodec/aarch64/hevcdsp_init_aarch64.c` 仅在 `bitDepth == 10` 且 CPU 支持
NEON 时安装函数指针。其他位深和不满足条件的平台继续走原实现。

`libavcodec/aarch64/hevcdsp_qpel_10_neon.c` 按 HEVC 规范保留两阶段定点计算：

1. 水平阶段按 qpel 八抽头或 epel 四抽头生成 16-bit 中间值；
2. 垂直阶段使用 widening multiply-accumulate，按原偏置和移位输出；
3. uni 路径做饱和窄化，bi 路径先与第一路预测相加再舍入；
4. 8-lane 为主循环，4-lane 和标量尾部覆盖所有合法块宽。

垂直 qpel 的 `filter8_shift6_2rows*` 同时计算相邻两行。两行窗口有七行输入
重叠，因此复用已加载向量，减少重复加载。二维 qpel/epel 仍使用 FFmpeg 规定
的临时缓冲区步长，未改变边界扩展和调用语义。

`libavcodec/aarch64/hevcdsp_qpel_10_neon_asm.S` 实现水平八抽头热点。普通相位
使用重叠向量加载；半像素相位的系数
`[-1, 4, -11, 40, 40, -11, 4, -1]` 对称，汇编先把对称像素配对，再乘四组
系数，缩短相关链并减少乘加指令。

## 覆盖范围与回退

- 覆盖 Main10 luma qpel 的 h/v/hv、uni/bi 和 chroma epel hv；
- 覆盖合法块宽及非 8 对齐尾部；
- 8-bit、9-bit、12-bit、非 AArch64 和无 NEON 环境保持原路径；
- 非本补丁覆盖的 epel 单方向函数仍使用原有实现，保证所有视频均可解码。

## 验证与性能

开发过程中的完整 15000 帧测试里，垂直双行复用使 real 从 33.265 s 降至
32.594 s；后续半像素水平对称核在相同代码栈上使 32.506 s 降至 32.261 s。
这两个数字是该补丁内部阶段数据，不应与其他补丁相加计算孤立收益。

前 300 帧 RGB24 `framemd5` 管道 SHA-256 为
`903d4493e85b12ed85ee76a4683065ef602c2546f19a1fa807ad8c8ad7f60543`，与基线
一致；3000 帧组合版与基线 SHA-256 同为
`9bf7a192699267ddfb6615887231209758dcdc695aa740c6ca75cb5f4709f792`。
