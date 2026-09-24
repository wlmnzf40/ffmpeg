# 优化点 8：HEVC 整数像素双向预测直接合并

- 提交：`f92e8a4`（`avcodec/hevc: fuse integer bidirectional prediction`）
- 补丁：[`0008-avcodec-hevc-fuse-integer-bidirectional-prediction.patch`](../../../patches/hevc-720p-aarch64/0008-avcodec-hevc-fuse-integer-bidirectional-prediction.patch)
- 前置：优化点 7；不依赖线程调节或 PGO

## 优化背景

未加权的双向预测在两路运动矢量都落到整数像素时，原路径仍执行两个独立 DSP：

1. `put_hevc_pel_pixels` 将第一路 8-bit 像素左移 6 位，写入步长固定为
   `MAX_PB_SIZE` 的 16-bit `lc->tmp`；
2. `put_hevc_pel_bi_pixels` 再读取第二路 8-bit 像素和 `lc->tmp`，相加、舍入、
   饱和并写入最终图像。

在 patch7 的 perf 中，32 像素宽的上述两个 NEON 核合计约占 8.5% cycles，
16 像素宽的两个核还占约 5%。中间块只用于把两个整数像素做舍入平均，因此
16-bit 扩展、临时写回和再次读回都可以消除。

## 等价变换

8-bit 原路径对每个像素的计算为：

```text
round(((src0 << 6) + (src1 << 6)) / 128)
= (src0 + src1 + 1) >> 1
```

AArch64 NEON 的 `URHADD` 对无符号 8-bit lane 恰好执行
`(a + b + 1) >> 1`。因此直接读取两路参考图并执行 `URHADD`，与原路径逐像素
bit-exact。

快速路径只在以下条件同时满足时使用：

- 没有加权预测；
- 两路 luma 的水平和垂直四分之一像素余数都为 0，或两路 chroma 的八分之一
  像素余数都为 0；
- 边界扩展已经按原路径完成，传入 DSP 的源指针与原逻辑一致。

其余情况完全执行原来的 `lc->tmp` 路径。

## 代码走读

### 1. 通用 DSP 接口与 C 参考实现

`libavcodec/hevc/dsp.h` 新增 `put_hevc_pel_bi_direct`，一次接收两路源指针、
各自步长、目标步长和块尺寸。

`libavcodec/hevc/dsp_template.c` 为 8/9/10/12-bit 提供通用 C 参考实现，直接
计算两路像素的舍入平均。`libavcodec/hevc/dsp.c` 在通用初始化时安装对应位深
实现，保证非 AArch64 平台和高位深路径语义完整。

### 2. luma/chroma 快速路径

`libavcodec/hevc/hevcdec.c` 在 `luma_mc_bi` 和 `chroma_mc_bi` 完成源地址与边界
扩展处理后检查整数像素条件。命中时直接调用新 DSP 并返回，不再生成或消费
`lc->tmp`。

检查放在边界处理之后很重要：位于画面边缘的块仍使用原有 edge-emulation
缓冲区，只改变最后的像素合并方式，不改变参考采样范围。

### 3. AArch64 NEON 实现

`libavcodec/aarch64/h26x/epel_neon.S` 新增
`ff_hevc_put_hevc_pel_bi_direct_8_neon`：

- 8/16/32/64 宽度使用专用逐行循环；
- 每 16 个像素只需两次向量加载、一次 `URHADD` 和一次向量存储；
- 4/6/12/24/48 等合法 HEVC 块宽使用通用 16/8/4/2 像素尾部路径；
- 不分配 16-bit 中间块，不访问 `lc->tmp`。

`libavcodec/aarch64/hevcdsp_init_aarch64.c` 仅在 8-bit 且 NEON 可用时覆盖通用
C 实现。

### 4. checkasm 覆盖

`tests/checkasm/hevc_pel.c` 新增两路独立随机输入，对全部 9 种合法块宽和
8～12-bit C 参考实现进行比较。AArch64 上专门覆盖 8-bit NEON 的通用尾部和
8/16/32/64 专用循环。

## 单项性能结果

基线为 patch1+2+5+6+7，候选再增加 patch8。双方均使用 `-threads 0`，不含
patch3/PGO。15 组非绑定 CPU 交替测试中位数：

| 指标 | patch1+2+5+6+7 | 再加 patch8 | 变化 |
|---|---:|---:|---:|
| real | 1.347 s | 1.269 s | 降低 5.77% |
| task-clock | 21618 ms | 20838 ms | 降低 3.61% |
| cycles | 62.339 G | 60.069 G | 降低 3.64% |
| instructions | 159.333 G | 150.047 G | 降低 5.83% |

固定在 CPU 0～31 的 21 组交替复测中，21/21 组四项 CPU 指标均胜出：

| 指标 | patch7 基线 | patch8 | 中位数变化 | 配对中位数变化 |
|---|---:|---:|---:|---:|
| real | 1.761 s | 1.680 s | -4.63% | -4.49% |
| task-clock | 29617 ms | 28592 ms | -3.46% | -3.34% |
| cycles | 85.451 G | 82.465 G | -3.49% | -3.38% |
| instructions | 159.437 G | 150.137 G | -5.83% | -5.83% |

## 最新累计结果

直接比较“BoostKit + patch1+2”和“再加 patch5+6+7+8”，15 组交替测试为：

| 指标 | patch1+2 基线 | patch1+2+5+6+7+8 | 变化 |
|---|---:|---:|---:|
| real | 1.455 s | 1.292 s | 降低 11.20% |
| task-clock | 24465 ms | 21056 ms | 降低 13.94% |
| cycles | 70.598 G | 60.711 G | 降低 14.01% |
| instructions | 195.793 G | 150.045 G | 降低 23.37% |

## 正确性与门禁

- AArch64 完整编译通过；
- `checkasm-hevc_pel` 的 `pel_bi_direct` 通过，全部 349 项测试通过；
- 完整 15000 帧 YUV420 `framecrc` 文本 SHA-256 仍为
  `b32a73512313062dd115911a0adfba24280292baf0404e5e05012ee3f11d1449`；
- 前 300 帧 RGB24 MD5 仍为 `8cbbf07aeeed01070fb7974b8f6a27f3`；
- 加权预测或任一路存在分数像素时严格回退到原路径。

## 被否决的前置尝试

在确定本优化前，曾测试 `ff_emulated_edge_mc_8` 的缓存边界值、打包写和 NEON
精确尾部版本。它们最多减少约 1.3% 指令，但纯 HEVC 解码 cycles 上升约 1%，
因此全部回退，未进入本补丁。
