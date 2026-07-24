# SM3 密码杂凑算法的 SIMD 软件实现与优化

基于 SIMD 寄存器与通用寄存器混合的国密 SM3 哈希算法高性能实现，支持 x86（SSE2/AVX2/AVX-512）和 ARM64（NEON）架构。

## 项目结构

```
├── README.md          本文件
├── Makefile           多架构构建系统
├── .gitignore
├── src/               源代码
│   ├── sm3.h          公共头文件：数据结构定义与 API 声明
│   ├── sm3_ref.c      标量参考实现（可移植 C，无 SIMD 依赖）
│   ├── sm3_hybrid.c   混合优化实现（SIMD 消息扩展 + GPR 压缩）
│   ├── sm3_avx2.c     AVX2 多缓冲实现（8 路并行，256-bit YMM）
│   ├── sm3_avx512.c   AVX-512 多缓冲实现（16 路并行，512-bit ZMM）
│   ├── sm3_neon.c     ARM NEON 多缓冲实现（4 路并行，128-bit Q）
│   └── main.c         测试框架：正确性验证 + 性能基准测试
└── doc/               文档
    └── SM3实验报告.pdf 实验报告
```

## 快速开始

### Linux x86-64

```bash
# 编译（自动检测架构，启用 AVX2）
make

# 运行测试与基准
./sm3_bench
```

### Windows (MinGW)

```bash
gcc -O2 -std=c99 -msse2 -mavx2 -Isrc -o sm3_bench.exe src/sm3_ref.c src/sm3_hybrid.c src/sm3_avx2.c src/main.c -lm
./sm3_bench.exe
```

### 其他编译选项

```bash
make ref          # 仅编译标量参考版本
make avx2         # x86-64 AVX2 版本
make avx512       # x86-64 AVX-512 版本（需支持 AVX-512F + AVX-512VL）
make neon         # ARM64 NEON 版本
make clean        # 清理编译产物
make info         # 查看当前编译配置
```

### 交叉编译 ARM64

```bash
make ARCH=arm64 CROSS=aarch64-linux-gnu- neon
```

## 优化策略

| 层次 | 策略 | 文件 | 说明 |
|---|---|---|---|
| 1 | 安全的循环左移 | sm3_ref.c, sm3_hybrid.c | `(x << (n&31)) \| (x >> ((-n)&31))` 消除 UB |
| 2 | 预计算轮常量 | 全部 | `T[j] ⋘ j` 共 64 个值编译期预计算 |
| 3 | 混合 SIMD+GPR | sm3_hybrid.c | 消息扩展 4 字一组 SIMD 并行，压缩用 GPR |
| 4 | 多缓冲全向量化 | sm3_avx2.c | SoA 布局，8 条消息同时压缩 |


## 参考文献

- GM/T 0004-2012, SM3 密码杂凑算法, 国家密码管理局, 2012
- [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/)
- [ARM NEON Intrinsics Reference](https://developer.arm.com/architectures/instruction-sets/intrinsics/)
