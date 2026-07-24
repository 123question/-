# SM4 对称密码算法优化实现

## 项目结构
lab3/
├── README.md               项目说明文档
├── Makefile                编译配置文件
├── include/                头文件目录
│   ├── sm4.h               主头文件，包含所有公共接口
│   ├── sm4_constants.h     S盒、FK、CK等常量定义
│   ├── sm4_baseline.h      基础实现接口
│   ├── sm4_ttable.h        T-table优化接口
│   ├── sm4_shuffle.h       Shuffle优化接口
│   ├── sm4_avx512.h        AVX-512优化接口
│   └── sm4_ctr.h           CTR模式接口
├── src/                    源代码目录
│   ├── sm4_baseline.c      基础实现
│   ├── sm4_ttable.c        T-table优化实现
│   ├── sm4_shuffle.c       Shuffle优化实现
│   ├── sm4_avx512.c        AVX-512优化实现
│   └── sm4_ctr.c           CTR模式实现
└── test/                   测试目录
    ├── test_sm4.c          单元测试（功能验证）
    └── test_perf.c         性能测试（速度测试）



## 优化方法

1. **基础实现 (Baseline)** - 作为性能对比基准
2. **T-table 优化** - 预计算S盒+线性变换，4次查表替代计算
3. **Shuffle 优化** - 比特切片技术，同时处理8个数据块
4. **AVX-512 优化** - 使用AVX-512指令集，支持16路并行处理（需CPU支持）
5. **CTR 工作模式** - 流模式加密
## 编译运行

```bash
# 编译所有
make

# 运行单元测试
make run_test

# 运行性能测试
make run_perf

# 清理
make clean
