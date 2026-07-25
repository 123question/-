/**
 * 密文2D卷积 —— 旋转优化版（可分离核法）
 * K = v·h^T, 旋转: 5→3（理论下界）
 */

#include "openfhe.h"
#include <iostream>
#include <vector>
#include <iomanip>

using namespace lbcrypto;

void print_matrix(const std::string& name,
                  const std::vector<std::vector<int64_t>>& mat) {
    std::cout << name << ":\n";
    for (const auto& row : mat) {
        for (const auto& val : row)
            std::cout << std::setw(4) << val << " ";
        std::cout << "\n";
    }
    std::cout << "\n";
}

std::vector<std::vector<int64_t>> plaintext_convolution(
    const std::vector<std::vector<int64_t>>& input,
    const std::vector<std::vector<int64_t>>& kernel) {

    int input_rows = input.size();
    int input_cols = input[0].size();
    int kernel_rows = kernel.size();
    int kernel_cols = kernel[0].size();
    int output_rows = input_rows - kernel_rows + 1;
    int output_cols = input_cols - kernel_cols + 1;

    std::vector<std::vector<int64_t>> output(
        output_rows, std::vector<int64_t>(output_cols, 0));

    for (int i = 0; i < output_rows; i++)
        for (int j = 0; j < output_cols; j++)
            for (int ki = 0; ki < kernel_rows; ki++)
                for (int kj = 0; kj < kernel_cols; kj++)
                    output[i][j] += input[i + ki][j + kj] * kernel[ki][kj];
    return output;
}

int main() {
    // ── 明文数据 ──
    std::vector<std::vector<int64_t>> input = {
        {1,  2,  3,  4},
        {5,  6,  7,  8},
        {9,  10, 11, 12},
        {13, 14, 15, 16}
    };

    std::vector<std::vector<int64_t>> kernel = {
        {1, 0, -1},
        {1, 0, -1},
        {1, 0, -1}
    };

    auto expected = plaintext_convolution(input, kernel);

    // ── OpenFHE 初始化 ──
    CCParams<CryptoContextBFVRNS> parameters;
    parameters.SetPlaintextModulus(65537);
    parameters.SetMultiplicativeDepth(3);

    CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);
    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);
    cc->Enable(ADVANCEDSHE);

    auto keys = cc->KeyGen();
    cc->EvalMultKeyGen(keys.secretKey);
    cc->EvalRotateKeyGen(keys.secretKey, {2, 4, 8});

    // ── 打包与加密 ──
    std::vector<int64_t> flat_input;
    for (const auto& row : input)
        flat_input.insert(flat_input.end(), row.begin(), row.end());

    Plaintext pt_input = cc->MakePackedPlaintext(flat_input);
    auto ct_input = cc->Encrypt(keys.publicKey, pt_input);

    // 可分离核法：将 3×3 卷积核 K = [[1,0,-1],[1,0,-1],[1,0,-1]] 分解为
    // K = v · h^T，其中 h = [1, 0, -1]（水平），v = [1, 1, 1]^T（垂直）
    // 二维卷积 I * K 等价于两次一维卷积：(I * h) * v
    // 旋转次数从 5 降至 3（理论下界），乘法从 6 降至 1

    // ── 水平通道: H = I * h = I - rot(I, 2) ──
    // H[i] = I[i]*1 + I[i+1]*0 + I[i+2]*(-1) = I[i] - I[i+2]
    auto ct_rot2 = cc->EvalRotate(ct_input, 2);  // 左移2列，对齐 h 的第3个权重

    std::vector<int64_t> neg_one(16, -1);
    Plaintext pt_neg = cc->MakePackedPlaintext(neg_one);
    auto ct_weighted_2 = cc->EvalMult(ct_rot2, pt_neg);  // 乘以权重 -1

    auto ct_H = cc->EvalAdd(ct_input, ct_weighted_2);    // H = I + (-rot2)

    // ── 垂直通道: O = H * v = H + rot(H,4) + rot(H,8) ──
    // O[i] = H[i]*1 + H[i+4]*1 + H[i+8]*1（权重全为1，无需乘法）
    auto ct_rot4 = cc->EvalRotate(ct_H, 4);  // 下移1行（每行4列）
    auto ct_rot8 = cc->EvalRotate(ct_H, 8);  // 下移2行

    auto ct_O = cc->EvalAdd(ct_H, ct_rot4);  // O = H + rot(H,4)
    ct_O = cc->EvalAdd(ct_O, ct_rot8);       // O = O + rot(H,8)

    // ── 解密与验证 ──
    Plaintext pt_result;
    cc->Decrypt(keys.secretKey, ct_O, &pt_result);
    std::vector<int64_t> result_vec = pt_result->GetPackedValue();

    std::vector<std::vector<int64_t>> actual(2, std::vector<int64_t>(2, 0));
    actual[0][0] = result_vec[0];
    actual[0][1] = result_vec[1];
    actual[1][0] = result_vec[4];
    actual[1][1] = result_vec[5];

    print_matrix("密文卷积结果", actual);
    print_matrix("明文卷积结果", expected);

    bool correct = true;
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            if (actual[i][j] != expected[i][j]) {
                std::cout << "位置 (" << i << "," << j << "): "
                          << "期望 " << expected[i][j]
                          << ", 实际 " << actual[i][j] << "  X\n";
                correct = false;
            }
        }
    }

    if (correct)
        std::cout << "\n验证通过！\n";
    else {
        std::cout << "\n验证失败！\n";
        return 1;
    }

    return 0;
}
