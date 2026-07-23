# SM4 对称密码算法优化实现

## 项目结构
sm4_optimized/
├── include/ # 头文件
├── src/ # 源代码
├── test/ # 测试程序
├── Makefile # 编译配置
└── README.md # 本文件

text

## 优化方法

1. **基础实现 (Baseline)** - 作为性能对比基准
2. **T-table 优化** - 预计算S盒+线性变换，4次查表替代计算
3. **Shuffle 优化** - 比特切片技术，同时处理8个数据块
4. **CTR 工作模式** - 流模式加密

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
