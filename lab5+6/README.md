# Encrypted 2D Convolution with BFV Fully Homomorphic Encryption

使用 **OpenFHE C++ 库** 和 **BFV 方案** 实现密文 2D 卷积。采用"打包→旋转→累加"策略，将 16 个像素打包为一个密文，利用 `EvalRotate` 对齐滑动窗口，高效完成同态卷积。

## 概述

全同态加密（FHE）允许直接在加密数据上进行计算。本项目包含两种密文卷积实现：

1. **HE_Conv.cpp** — 逐核权重法：对 9 个核位置逐一旋转、乘权重、累加
2. **HE_Conv_optimized.cpp** — 可分离核法：利用 `K = v·h^T` 分解，将 2D 卷积降为两次 1D 卷积，旋转次数从 5 降至 3（理论下界）

## 卷积参数

| 参数 | 值 |
|------|-----|
| 输入 | 4 × 4 矩阵 |
| 卷积核 | 3 × 3 (Sobel 水平边缘检测) |
| 步长 | 1 |
| 填充 | 0 (valid) |
| 输出 | 2 × 2 矩阵 |

## 策略说明

### 逐核权重法（HE_Conv.cpp）

1. **打包**: 将 4×4 输入按行优先展平为 16 维向量，编码为单个明文多项式并加密
2. **旋转**: 对 9 个核位置 $(k_i, k_j)$，用 `EvalRotate(ct, k_i \times 4 + k_j)$ 旋转密文，使滑动窗口对齐
3. **累加**: 旋转后的密文乘以核权重后累加到结果密文

### 可分离核法（HE_Conv_optimized.cpp）

卷积核 `K = [[1,0,-1],[1,0,-1],[1,0,-1]]` 可分解为 `K = v·h^T`，其中：
- `h = [1, 0, -1]`（水平方向）
- `v = [1, 1, 1]^T`（垂直方向）

2D 卷积 `I * K` 等价于两次 1D 卷积 `(I * h) * v`：
- **水平通道**: `H = I * h = I - rot(I, 2)`（1 次旋转 + 1 次乘法 + 1 次加法）
- **垂直通道**: `O = H * v = H + rot(H, 4) + rot(H, 8)`（2 次旋转 + 2 次加法，权重全为 1 无需乘法）

## 快速开始

### 前置条件

- **Linux** (Ubuntu 20.04 或更新)
- C++ 编译器 (GCC 9+, 支持 C++17)
- CMake 3.16+
- GMP 库 (`libgmp-dev`)

### 方式一: 在 OpenFHE 源码 build 目录直接编译

```bash
# 1. 克隆并编译 OpenFHE C++ 库
git clone https://github.com/openfheorg/openfhe-development.git
cd openfhe-development
git checkout v1.2.3
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DWITH_OPENMP=OFF -DWITH_NATIVEOPT=ON
make -j$(nproc)

# 2. 将源文件复制到 build 目录
cp /path/to/fhe-convolutionUbuntu20.04/HE_Conv.cpp .
cp /path/to/fhe-convolutionUbuntu20.04/HE_Conv_optimized.cpp .

# 3. 编译逐核权重法
g++ -std=c++17 -O2 \
    -I../src/core/include \
    -I../src/pke/include \
    -I../src/binfhe/include \
    -I../third-party/cereal/include \
    -I./src/core \
    -L./lib \
    HE_Conv.cpp \
    -lOPENFHEpke -lOPENFHEcore -lOPENFHEbinfhe \
    -lpthread \
    -o HE_Conv

# 4. 编译可分离核法
g++ -std=c++17 -O2 \
    -I../src/core/include \
    -I../src/pke/include \
    -I../src/binfhe/include \
    -I../third-party/cereal/include \
    -I./src/core \
    -L./lib \
    HE_Conv_optimized.cpp \
    -lOPENFHEpke -lOPENFHEcore -lOPENFHEbinfhe \
    -lpthread \
    -o HE_Conv_optimized

# 5. 运行
export LD_LIBRARY_PATH="./lib:$LD_LIBRARY_PATH"
./HE_Conv
./HE_Conv_optimized
```


## 项目结构

```
fhe-convolutionUbuntu20.04/
├── README.md
├── HE_Conv.cpp                 # 逐核权重法（5 次旋转）
├── HE_Conv_optimized.cpp       # 可分离核法（3 次旋转，理论下界）
└── report/
    └── report.pdf              # 实验报告
```

## 库信息

- **OpenFHE**: <https://github.com/openfheorg/openfhe-development>
- **方案**: BFV (Brakerski-Fan-Vercauteren) RNS 变体
