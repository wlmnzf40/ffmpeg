# 鲲鹏 920 静态 FFmpeg 库构建与 benchmark

以下命令在全部代码 patch 应用后执行。`--disable-shared --enable-static` 让 FFmpeg
自身各库静态链接进可执行文件，避免运行时找不到 `libavcodec.so`、
`libavformat.so` 等错误；系统的 glibc、libomp 仍由操作系统提供。

## Clang 17 + LTO（当前最快）

```bash
make distclean
./configure \
    --prefix="$PWD/install-clang-lto" \
    --cc=clang --cxx=clang++ \
    --ar=llvm-ar --ranlib=llvm-ranlib --nm=llvm-nm \
    --disable-shared --enable-static \
    --enable-pthreads --enable-gpl --disable-doc \
    --enable-lto \
    --extra-cflags='-fopenmp -O3 -mcpu=tsv110' \
    --extra-ldflags='-fopenmp'
make -j"$(nproc)" ffmpeg
```

不要给最终 LTO 链接强行追加 `-mcpu=tsv110`。虽然各编译单元已经使用该参数，
本机完整成对测试显示把 LTO plugin 从 generic 改为 tsv110 反而略慢。使用
`llvm-ar`、`llvm-ranlib`、`llvm-nm` 可避免 GNU `ar` 的 “plugin needed to
handle lto object” 问题。

## GCC 非 LTO 对照

```bash
make distclean
./configure \
    --prefix="$PWD/install-gcc" \
    --disable-shared --enable-static \
    --enable-pthreads --enable-gpl --disable-doc \
    --extra-cflags='-fopenmp -O3 -mcpu=tsv110' \
    --extra-ldflags='-fopenmp'
make -j"$(nproc)" ffmpeg
```

## 正确性和完整 benchmark

```bash
./ffmpeg -nostdin -v error -threads 0 \
    -i /data/wanglimin/HEVC-720p-10min.mov \
    -frames:v 300 -an -sn -vf format=rgb24 \
    -f framemd5 - 2>/dev/null | sha256sum

time ./ffmpeg -nostdin -stats -hide_banner -threads 0 \
    -i /data/wanglimin/HEVC-720p-10min.mov \
    -frames:v 15000 -an -sn -vf format=rgb24 -f null -
```

预期 300 帧哈希为
`903d4493e85b12ed85ee76a4683065ef602c2546f19a1fa807ad8c8ad7f60543`。
性能数值必须在同一台机器、相同负载下交替复测；当前 Clang LTO 代表值为
29.563 s，应用 patch19 后的 GCC 代表值为 30.384 s。
