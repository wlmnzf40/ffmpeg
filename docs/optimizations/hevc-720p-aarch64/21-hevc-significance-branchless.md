# 优化点 21：HEVC 显著系数标志的无分支索引写入

## 代码走读

`DecodeSignificantCoeffFlagsLocal` 按扫描顺序逐个读取 CABAC 标志。
原实现每读一个标志就分支判断是否把扫描索引写入数组；该分支位于
CABAC 串行热循环中。现在先把索引写到当前 `count` 位置，再按标志值
（0 或 1）增加 `count`。标志为 0 时，下次迭代会覆盖同一位置；
标志为 1 时，该索引保留。循环的读位顺序、CABAC 状态及最终索引序列不变。

只修改 GCC/AArch64 使用的显著系数 helper，不改变线程数或解码策略。
候选先单独叠加到已提交的 patch20 源码，后续其它 CABAC 改动均未加入此对照。

## 验证

2026-09-26 从 patch20 与本补丁对应的 Git 提交重新导出干净源码，
在鲲鹏 920 上以相同 GCC 静态配置构建。输入为
`/data/wanglimin/HEVC-720p-10min.mov`；为排除仓库早期自动线程
补丁的影响，两版均固定 16 个解码线程（进程均为 52 个线程）：

```bash
ffmpeg -nostdin -v fatal -threads 16 \
    -i /data/wanglimin/HEVC-720p-10min.mov \
    -frames:v 15000 -an -sn -vf format=rgb24 -f null -
```

| 交叉顺序 | patch20 `real` | 本优化 `real` |
|---|---:|---:|
| patch20 → 本优化 | 30.333 s | 29.684 s |
| 本优化 → patch20 | 30.794 s | 29.996 s |

均值约下降 0.724 秒（2.37%）。原报告的 `-threads 0` 数字来自
混有未提交试验代码的构建，已由以上干净构建结果取代。3000 帧 RGB24 `framemd5` 管道
SHA-256 与 patch20 相同：
`9bf7a192699267ddfb6615887231209758dcdc695aa740c6ca75cb5f4709f792`。
这仍未达到 20 秒目标，也不能外推到其他输入或编译器。
