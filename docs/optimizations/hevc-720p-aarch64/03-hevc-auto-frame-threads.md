# 优化点 3：HEVC 自动帧线程上限

- 提交：`a7edeb5`（`avcodec: raise automatic HEVC frame threads to 32`）
- 补丁：[`0003-avcodec-raise-automatic-HEVC-frame-threads-to-32.patch`](../../../patches/hevc-720p-aarch64/0003-avcodec-raise-automatic-HEVC-frame-threads-to-32.patch)

## 问题

FFmpeg 7.1.1 在 `libavcodec/pthread_internal.h` 中把所有 codec 的自动线程数
统一限制为 16。这个限制最初用于规避 H.264 slice threading 在高线程数下的
问题，但同样限制了支持 frame threading 的 HEVC 解码器。

鲲鹏 920B 测试机有 256 个逻辑 CPU。RGB24 NEON 完成后，16 个 HEVC 帧线程
成为新的主要瓶颈。

## 代码走读

改动保留原有 `MAX_AUTO_THREADS=16`，只新增 HEVC 专用上限 32：

```c
#define MAX_AUTO_THREADS 16
#define MAX_HEVC_AUTO_THREADS 32
```

`ff_frame_thread_init()` 在用户没有显式设置 `-threads` 时，根据
`avctx->codec_id` 选择上限：

- `AV_CODEC_ID_HEVC`：最多 32；
- 其他 codec：仍为 16；
- 用户显式传入线程数：完全不受此自动策略改动影响。

这样不会放宽 H.264 的既有限制，也不会改变低核机器行为。

## 性能依据

纯解码测试的代表值：

| HEVC 线程 | real |
|---:|---:|
| 16 | 约 1.34 s |
| 20 | 约 1.11 s |
| 32 | 约 1.05 s |
| 48 | 约 1.12 s |

32 是本机较稳定的折中点；继续增加线程会增加帧缓存、同步和调度开销。

## 正确性

旧版和 32 自动线程版对前 100 帧原生 YUV420 输出的 MD5 相同：

```text
MD5=d3d5e73ee391e55b5f8b76fa25231130
```
