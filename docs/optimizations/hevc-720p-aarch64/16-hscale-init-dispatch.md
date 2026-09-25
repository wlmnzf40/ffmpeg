# 优化点 16：初始化阶段分派恒等 hscale

- 提交：`76321fecd`（`swscale/aarch64: select identity hscale at initialization`）
- 补丁：[`0016-swscale-aarch64-select-identity-hscale-at-initializa.patch`](../../../patches/hevc-720p-aarch64/0016-swscale-aarch64-select-identity-hscale-at-initializa.patch)
- 前置：优化点 15

## 优化背景

优化点 11 增加了 16-bit 输入恒等缩放 NEON 路径，但包装函数在每一行都重新读取
像素格式描述符，并检查滤波长度、首项、中项和末项。滤波器和像素格式在一个
`SwsContext` 生命周期内不变，这些判断不应位于逐行热路径。

## 代码走读

`hscale16to15_shift` 集中计算原实现的位深移位，保留 RGB、PAL8、单色和浮点格式
的特殊规则。

`ff_sws_init_swscale_aarch64` 在安装 NEON 函数时只检查一次亮度和色度滤波器：

- 恒等滤波器直接把 `hyScale` 或 `hcScale` 指向
  `hscale16to15_identity_neon`；
- 其它滤波器继续使用 `ff_hscale16to15_4_neon_asm`；
- 函数签名保持与 swscale 分派表一致，恒等内核只是忽略不需要的滤波参数。

这样没有增加视频特例，也不会因为换分辨率或滤波器而错误命中。

## 验证与性能

输出哈希与优化点 15 一致。5 组成对硬件计数中 3 组周期下降，周期中位数约
下降 0.43%，指令数稳定下降约 0.67%。完整测试代表值为 30.718 s，对照热态
代表值为 30.817 s。
