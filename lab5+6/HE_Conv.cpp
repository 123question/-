/**
 * 作业5: 全同态加密卷积
 * 
 * 任务: 使用单输入单输出 4×4 输入和 3×3 卷积核
 * 参数: 步长1，无填充
 * 
 * 方案: BFV (整数算术)
 * 库: OpenFHE
 */

#include "openfhe.h"
#include <iostream>
#include <vector>
#include <iomanip>
#include <algorithm>  

using namespace lbcrypto;

// ============================================================
// 辅助函数: 打印矩阵
// ============================================================

void print_matrix(const std::string& name, const std::vector<std::vector<int64_t>>& mat) {
    std::cout << name << ":\n";
    for (const auto& row : mat) {
        for (const auto& val : row) {
            std::cout << std::setw(4) << val << " ";
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

// ============================================================
// 明文卷积 (验证用)
// ============================================================

std::vector<std::vector<int64_t>> plaintext_convolution(
    const std::vector<std::vector<int64_t>>& input,
    const std::vector<std::vector<int64_t>>& kernel) {
    
    int input_rows = input.size();
    int input_cols = input[0].size();
    int kernel_rows = kernel.size();
    int kernel_cols = kernel[0].size();
    
    int output_rows = input_rows - kernel_rows + 1;
    int output_cols = input_cols - kernel_cols + 1;
    
    std::vector<std::vector<int64_t>> output(output_rows, std::vector<int64_t>(output_cols, 0));
    
    for (int i = 0; i < output_rows; i++) {
        for (int j = 0; j < output_cols; j++) {
            int64_t sum = 0;
            for (int ki = 0; ki < kernel_rows; ki++) {
                for (int kj = 0; kj < kernel_cols; kj++) {
                    sum += input[i + ki][j + kj] * kernel[ki][kj];
                }
            }
            output[i][j] = sum;
        }
    }
    return output;
}

// ============================================================
// 主函数: 密文卷积
// ============================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "  密文卷积 (BFV 方案)\n";
    std::cout << "  4x4 输入 × 3x3 卷积核\n";
    std::cout << "  步长 1, 无填充\n";
    std::cout << "========================================\n\n";

    // ============================================================
    // 1. 明文数据
    // ============================================================
    
    std::vector<std::vector<int64_t>> input = {
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 10, 11, 12},
        {13, 14, 15, 16}
    };
    
    std::vector<std::vector<int64_t>> kernel = {
        {1, 0, -1},
        {1, 0, -1},
        {1, 0, -1}
    };
    
    print_matrix("输入矩阵 (4x4)", input);
    print_matrix("卷积核 (3x3)", kernel);

    // ============================================================
    // 2. 明文卷积验证
    // ============================================================
    
    auto expected = plaintext_convolution(input, kernel);
    print_matrix("明文卷积结果 (2x2)", expected);

    // ============================================================
    // 3. OpenFHE 参数设置
    // ============================================================
    
    std::cout << "--- 初始化 OpenFHE ---\n";
    
    CCParams<CryptoContextBFVRNS> parameters;
    parameters.SetPlaintextModulus(65537);
    parameters.SetMultiplicativeDepth(3);
    
    CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);
    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);
    cc->Enable(ADVANCEDSHE);  // 添加这行以支持更多操作
    
    std::cout << "  参数设置完成\n";
    std::cout << "  Ring Dimension: " << cc->GetRingDimension() << "\n";
    std::cout << "  Batch Size: " << cc->GetRingDimension() / 2 << "\n\n";

    // ============================================================
    // 4. 密钥生成
    // ============================================================
    
    std::cout << "--- 生成密钥 ---\n";
    
    auto keys = cc->KeyGen();
    cc->EvalMultKeyGen(keys.secretKey);
    
    // 生成旋转密钥，索引从 -15 到 15
    std::vector<int32_t> rot_indices;
    for (int i = -15; i <= 15; i++) {
        if (i != 0) {
            rot_indices.push_back(i);
        }
    }
    cc->EvalRotateKeyGen(keys.secretKey, rot_indices);
    
    std::cout << "  密钥生成完成\n\n";

    // ============================================================
    // 5. 数据编码与加密
    // ============================================================
    
    std::cout << "--- 编码与加密 ---\n";
    
    // 将输入矩阵展平为向量 (行优先)
    std::vector<int64_t> flat_input;
    for (const auto& row : input) {
        flat_input.insert(flat_input.end(), row.begin(), row.end());
    }
    
    // 将卷积核展平为向量 (行优先)
    std::vector<int64_t> flat_kernel;
    for (const auto& row : kernel) {
        flat_kernel.insert(flat_kernel.end(), row.begin(), row.end());
    }
    
    // 编码为明文
    Plaintext pt_input = cc->MakePackedPlaintext(flat_input);
    
    // 加密输入
    auto ct_input = cc->Encrypt(keys.publicKey, pt_input);
    
    std::cout << "  加密完成\n\n";

    // ============================================================
    // 6. 密文卷积 (改进的旋转-累加方法)
    // ============================================================
    
    std::cout << "--- 执行密文卷积 ---\n";
    
    // 初始化结果密文为零
    std::vector<int64_t> zeros(16, 0);
    Plaintext pt_zero = cc->MakePackedPlaintext(zeros);
    auto ct_result = cc->Encrypt(keys.publicKey, pt_zero);
    
    for (int ki = 0; ki < 3; ki++) {
        for (int kj = 0; kj < 3; kj++) {
            int64_t weight = flat_kernel[ki * 3 + kj];
            if (weight == 0) {
                continue;  // 跳过零权重
            }
            
            // 计算旋转量：当前位置相对于输出位置的偏移
            // 对于输出位置(0,0)，需要将输入旋转使得(i+ki, j+kj)对齐到(0,0)
            // 旋转量 = ki * 4 + kj（正向旋转，使得后面的元素移到前面）
            int shift = ki * 4 + kj;
            
            std::cout << "  处理 kernel[" << ki << "][" << kj << "] = " 
                      << weight << ", shift = " << shift << "\n";
            
            Ciphertext<DCRTPoly> ct_shifted;
            
            if (shift == 0) {
                ct_shifted = ct_input;
            } else {
                // 正向旋转：将后面的元素移到前面
                ct_shifted = cc->EvalRotate(ct_input, shift);
            }
            
            // 乘以权重
            std::vector<int64_t> weight_vec(16, weight);
            Plaintext pt_weight = cc->MakePackedPlaintext(weight_vec);
            auto ct_weighted = cc->EvalMult(ct_shifted, pt_weight);
            
            // 累加到结果
            ct_result = cc->EvalAdd(ct_result, ct_weighted);
        }
    }
    
    std::cout << "  卷积计算完成\n\n";

    // ============================================================
    // 7. 解密与解码
    // ============================================================
    
    std::cout << "--- 解密与解码 ---\n";
    
    Plaintext pt_result;
    cc->Decrypt(keys.secretKey, ct_result, &pt_result);
    
    std::vector<int64_t> result_vec = pt_result->GetPackedValue();
    
    // 显示完整的结果向量
    std::cout << "  完整结果向量: ";
    for (size_t i = 0; i < result_vec.size(); i++) {
        std::cout << result_vec[i] << " ";
    }
    std::cout << "\n\n";
    
    // 提取 2x2 结果
    // 使用正向旋转后，卷积结果应该位于向量的前4个位置（按行优先）
    std::vector<std::vector<int64_t>> actual(2, std::vector<int64_t>(2, 0));
    if (result_vec.size() >= 4) {
        actual[0][0] = result_vec[0];  // 输出(0,0)
        actual[0][1] = result_vec[1];  // 输出(0,1)
        actual[1][0] = result_vec[4];  // 输出(1,0)  
        actual[1][1] = result_vec[5];  // 输出(1,1)
    }
    
    print_matrix("密文卷积结果 (2x2)", actual);

    // ============================================================
    // 8. 验证正确性
    // ============================================================
    
    std::cout << "--- 验证结果 ---\n";
    
    bool correct = true;
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            if (actual[i][j] != expected[i][j]) {
                std::cout << "  位置 (" << i << "," << j << "): "
                          << "期望 " << expected[i][j] 
                          << ", 实际 " << actual[i][j] << " 错误\n";
                correct = false;
            }
        }
    }
    
    if (correct) {
        std::cout << "\n验证通过！密文卷积结果与明文卷积完全一致！\n\n";
    } else {
        std::cout << "\n验证失败！请检查实现。\n\n";
    }
    
    return 0;
}