# 优化点 4：鲲鹏 920B 定向 PGO/LTO 构建

- 提交：`96a81a7`（`tools: add Kunpeng HEVC PGO/LTO build workflow`）
- 补丁：[`0004-tools-add-Kunpeng-HEVC-PGO-LTO-build-workflow.patch`](../../../patches/hevc-720p-aarch64/0004-tools-add-Kunpeng-HEVC-PGO-LTO-build-workflow.patch)

## 目标

NEON 和线程优化后，热点集中在 HEVC 边界强度、环路滤波、运动补偿及少量
RGB24 NEON。通用 `-O3` 无法利用实际分支概率和调用频率。本优化增加可复现的
两阶段构建脚本，用目标视频训练 GCC profile，再进行 profile-use 和 LTO。

## 构建流程走读

`tools/build_kunpeng_hevc_pgo.sh` 使用同一个 out-of-tree build 目录，保证 GCC
生成和消费 profile 时的对象路径一致：

1. generation 阶段使用 `-mcpu=tsv110 -fprofile-generate` 编译。
2. 用正式 benchmark 的 `HEVC → yuv420p → rgb24` 路径训练 15000 帧。
3. 检查训练目录至少产生一个 `.gcda`，防止静默生成无 profile 的二进制。
4. `make distclean` 后在同一 build 目录重新 configure。
5. use 阶段启用 `-fprofile-use -fprofile-correction`、`-mcpu=tsv110` 和并行
   `-flto=32`。
6. 两个阶段均使用 `--disable-shared --enable-static`，把 FFmpeg 自有库静态链接
   进可执行文件；训练及最终运行不再依赖 `LD_LIBRARY_PATH`。
7. 静态 LTO 使用 `gcc-ar`、`gcc-ranlib` 和 `gcc-nm`，确保归档索引能够识别
   GCC LTO object。
8. 安装到用户指定 prefix，并检查最终程序没有依赖 `libav*.so`、
   `libswscale.so` 或 `libswresample.so`。

脚本拒绝使用非空工作目录，避免误删或覆盖已有构建数据。线程数、LTO 分区数
和训练帧数可由 `JOBS`、`LTO_JOBS`、`TRAINING_FRAMES` 环境变量调整。

这里的“静态”特指 FFmpeg 自有库静态链接。glibc、libm、libpthread 和 libgomp
等系统运行库仍由系统动态提供，避免完全静态链接 glibc 带来的 NSS、DNS 和系统
兼容性问题。

如果训练已经完成、仅在第二阶段静态 LTO 链接失败，可以保留工作目录中的
`profile/`，通过 `USE_EXISTING_PROFILE=1` 跳过 generation 和训练，直接重新执行
profile-use 构建。

## 性能影响

在没有并行编译负载的独立三轮中，PGO 版本为：

```text
1.055 s
1.093 s
1.097 s
平均 1.082 s
```

对应平均 user time 为 21.78 s。相对非 PGO 最终构建约 1.136 s，PGO/LTO
进一步改善约 4.8%。

## 使用方法

```bash
tmux new-session -d -s LLM-ffmpeg-pgo \
  "JOBS=64 LTO_JOBS=32 tools/build_kunpeng_hevc_pgo.sh \
   /root/ffmpeg-920b/HEVC-720p-10min.MOV \
   /root/ffmpeg-920b/pgo-work \
   /root/ffmpeg-920b/pgo-install"
```

安装完成后可直接运行，不需要设置 `LD_LIBRARY_PATH`：

```bash
/root/ffmpeg-920b/pgo-install/bin/ffmpeg -version
```

复用已经生成的 profile，仅重跑第二阶段：

```bash
USE_EXISTING_PROFILE=1 JOBS=64 LTO_JOBS=32 \
  bash tools/build_kunpeng_hevc_pgo.sh \
  /root/ffmpeg-920b/HEVC-720p-10min.MOV \
  /root/ffmpeg-920b/pgo-work \
  /root/ffmpeg-920b/pgo-install
```
