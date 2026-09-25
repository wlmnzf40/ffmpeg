# 优化点 23：HEVC 反量化系数的直接饱和裁剪

## 代码走读

`ff_hevc_hls_residual_coding` 计算有符号反量化系数后，需要写入 16-bit
系数缓冲区。旧代码按正负分别判断，再用大位掩码识别越界值。
新代码把相同语义直接写成 `[-32768, 32767]` 区间裁剪：

```c
trans_coeff_level = FFMAX(FFMIN(trans_coeff_level, 32767), -32768);
```

对于负数，旧表达式的 `~value` 超出 15 位当且仅当
`value < -32768`；对于非负数，越界条件是 `value > 32767`。
因此新旧结果相同。这个表达式让 GCC 对两个边界条件直接生成选择操作，
减少热循环中的条件控制流。只修改裁剪，不改变 CABAC 位读取、线程数或输出精度。

## 验证

本优化单独叠加在优化点 21、22 后。两版都是鲲鹏 920 上的静态 GCC
构建，固定原 benchmark 的 `-threads 0`：

```bash
ffmpeg -nostdin -v fatal -threads 0 \
    -i /data/wanglimin/HEVC-720p-10min.mov \
    -frames:v 15000 -an -sn -vf format=rgb24 -f null -
```

| 交叉顺序 | 优化点 22 后 `real` | 再加本优化 `real` |
|---|---:|---:|
| 对照 → 本优化 | 29.384 s | 28.691 s |
| 本优化 → 对照 | 29.369 s | 28.674 s |

均值约下降 0.694 秒（2.36%）。3000 帧 RGB24 `framemd5` 管道
SHA-256 与对照相同：
`9bf7a192699267ddfb6615887231209758dcdc695aa740c6ca75cb5f4709f792`。
独立测试“符号位无分支取反”未见收益，因此没有并入本提交。
完整 `real` 仍约 28.7 秒，尚未达到 20 秒目标。
