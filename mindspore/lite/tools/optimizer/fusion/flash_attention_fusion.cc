/**
 * Copyright 2023 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define USE_DEPRECATED_API
#include "tools/optimizer/fusion/flash_attention_fusion.h"
#include <memory>
#include "ops/sequence_ops.h"
#include "ops/op_utils.h"
#include "ops/array_ops.h"
#include "ops/nn_ops.h"
#include "tools/optimizer/common/gllo_utils.h"
#include "mindspore/core/ops/lite_ops.h"
#include "ops/incre_flash_attention.h"
#include "ops/prompt_flash_attention.h"

namespace mindspore::opt {
namespace {
constexpr auto kNameFlashAttentionPatternForSDBSH = "FlashAttentionPatternForSDBSH";
constexpr auto kNameFlashAttentionPatternForSDBNSD = "FlashAttentionPatternForSDBNSD";
constexpr auto kNameFlashAttentionPatternForPg = "FlashAttentionPatternForPg";
constexpr auto kNameFlashAttentionPatternForLLAMAPatternV1 = "FlashAttentionPatternForLLAMAPatternV1";
constexpr auto kNameFlashAttentionPatternForLLAMAPatternV2 = "FlashAttentionPatternForLLAMAPatternV2";
constexpr size_t kNumIndex0 = 0;
constexpr size_t kNumIndex1 = 1;
constexpr size_t kNumIndex2 = 2;
constexpr size_t kNumIndex3 = 3;
constexpr size_t kNumDimSize4 = 4;
constexpr size_t kNumShapeSize4 = 4;
constexpr int64_t kNumMinSeqLenSize = 1024;
constexpr int64_t kNumMaxNextTokenSize = 65535;
constexpr int kNumMultiple32 = 32;
constexpr int kNumMultiple16 = 16;

bool PFACheckShape(float scale_value, int64_t q_seq_len = 0, int64_t k_seq_len = 0, int64_t v_seq_len = 0,
                   int64_t d_value = 0) {
  if (q_seq_len != 0 && k_seq_len != 0 && v_seq_len != 0 && d_value != 0) {
    // for static shape
    if (q_seq_len == k_seq_len && q_seq_len == v_seq_len) {
      // for equal seq len
      if (static_cast<int>(d_value) % kNumMultiple16 != 0) {
        MS_LOG(INFO) << "for static shape: now D value must be an integer multiple of 16.";
        return false;
      }
    }
    if (q_seq_len != k_seq_len || q_seq_len != v_seq_len) {
      // for not equal seq len
      if (static_cast<int>(d_value) % kNumMultiple32 != 0) {
        MS_LOG(INFO) << "for static shape: now D value must be an integer multiple of 16.";
        return false;
      }
    }
    if (q_seq_len < kNumMinSeqLenSize || k_seq_len < kNumMinSeqLenSize) {
      MS_LOG(INFO) << "input tensor seq len is less 1024, not need fusion.";
      return false;
    }
  }
  if (q_seq_len == 0) {
    // for dynamic shape, can not check seq len, so D value must be an integer multiple of 32.
    float d = 1 / pow(scale_value, 2);
    if (ceil(d) != floor(d)) {
      MS_LOG(WARNING) << "cann not support, in dynamic rang shape";
      return false;
    }
    if (static_cast<int>(d) % kNumMultiple32 != 0) {
      MS_LOG(WARNING) << "for static shape: now D value must be an integer multiple of 32.";
      return false;
    }
  }
  return true;
}

int GetNumHeadForSD(const AnfNodePtr &q_trans_reshape) {
  auto concat_cnode = q_trans_reshape->cast<CNodePtr>()->input(kNumIndex2)->cast<CNodePtr>();
  if (concat_cnode == nullptr) {
    return -1;
  }
  auto concat_const_input = concat_cnode->input(kNumIndex3);
  if (!utils::isa<ParameterPtr>(concat_const_input)) {
    return -1;
  }
  auto concat_param = concat_cnode->input(kNumIndex3)->cast<ParameterPtr>()->default_param();
  if (concat_param == nullptr) {
    return -1;
  }
  auto concat_value = std::dynamic_pointer_cast<tensor::Tensor>(concat_param);
  if (concat_value == nullptr) {
    return -1;
  }
  auto concat_data = reinterpret_cast<int32_t *>(concat_value->data_c());
  if (concat_data == nullptr) {
    return -1;
  }
  return static_cast<int64_t>(concat_data[0]);
}

std::vector<int64_t> GetTensorShape(CNodePtr cnode, size_t input_index) {
  auto abstract = GetCNodeInputAbstract(cnode, input_index);
  if (abstract == nullptr) {
    MS_LOG(ERROR) << "GetCNodeInputAbstract in promapt flash attention fusion.";
    return {};
  }
  std::vector<int64_t> shape = {};
  if (FetchShapeFromAbstract(abstract, &shape) != lite::RET_OK) {
    MS_LOG(ERROR) << "FetchShapeFromAbstract failed.";
    return {};
  }
  return shape;
}
}  // namespace

std::unordered_map<std::string, VectorRef> FlashAttentionFusion::DefinePatterns() const {
  std::unordered_map<std::string, VectorRef> patterns;
  patterns[kNameFlashAttentionPatternForSDBSH] = DefineFlashAttentionPatternForSDBSH();
  patterns[kNameFlashAttentionPatternForSDBNSD] = DefineFlashAttentionPatternForSDBNSD();
  patterns[kNameFlashAttentionPatternForPg] = DefineFlashAttentionPatternForPg();
  patterns[kNameFlashAttentionPatternForLLAMAPatternV1] = DefineFlashAttentionPatternForLLAMAPatternV1();
  patterns[kNameFlashAttentionPatternForLLAMAPatternV2] = DefineFlashAttentionPatternForLLAMAPatternV2();
  return patterns;
}

const VectorRef FlashAttentionFusion::DefineFlashAttentionPatternForSDBNSD() const {
  // Q reshape
  auto reshape_q_input_1 = std::make_shared<Var>();  // input Q
  MS_CHECK_TRUE_RET(reshape_q_input_1 != nullptr, {});
  auto reshape_q_input_2 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(reshape_q_input_2 != nullptr, {});
  auto is_reshape_q = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimReshape>);
  MS_CHECK_TRUE_RET(is_reshape_q != nullptr, {});
  auto reshape_q = VectorRef({is_reshape_q, reshape_q_input_1, reshape_q_input_2});
  // K reshape
  auto reshape_k_input_1 = std::make_shared<Var>();  // input K
  MS_CHECK_TRUE_RET(reshape_k_input_1 != nullptr, {});
  auto reshape_k_input_2 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(reshape_k_input_2 != nullptr, {});
  auto is_reshape_k = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimReshape>);
  MS_CHECK_TRUE_RET(is_reshape_k != nullptr, {});
  auto reshape_k = VectorRef({is_reshape_k, reshape_k_input_1, reshape_k_input_2});
  // transpose
  auto is_transpose_param = std::make_shared<CondVar>(IsParamNode);
  MS_CHECK_TRUE_RET(is_transpose_param != nullptr, {});
  auto is_transpose = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimTranspose>);
  MS_CHECK_TRUE_RET(is_transpose != nullptr, {});
  auto transpose = VectorRef({is_transpose, reshape_k, is_transpose_param});
  // matmul 1
  auto is_matmul_1 = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimMatMulFusion>);
  MS_CHECK_TRUE_RET(is_matmul_1 != nullptr, {});
  auto matmul_1 = VectorRef({is_matmul_1, reshape_q, transpose});
  // mul
  auto is_mul_param = std::make_shared<CondVar>(IsParamNode);
  MS_CHECK_TRUE_RET(is_mul_param != nullptr, {});
  auto is_mul = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimMulFusion>);
  MS_CHECK_TRUE_RET(is_mul != nullptr, {});
  auto mul = VectorRef({is_mul, matmul_1, is_mul_param});
  // softmax
  auto is_softmax = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimSoftmax>);
  MS_CHECK_TRUE_RET(is_softmax != nullptr, {});
  auto softmax = VectorRef({is_softmax, mul});
  // cast
  auto is_cast_param = std::make_shared<CondVar>(IsParamNode);
  MS_CHECK_TRUE_RET(is_cast_param != nullptr, {});
  auto is_cast = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimCast>);
  MS_CHECK_TRUE_RET(is_cast != nullptr, {});
  auto cast = VectorRef({is_cast, softmax, is_cast_param});
  // V reshape
  auto reshape_v_input_1 = std::make_shared<Var>();  // input V
  MS_CHECK_TRUE_RET(reshape_v_input_1 != nullptr, {});
  auto reshape_v_input_2 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(reshape_v_input_2 != nullptr, {});
  auto is_reshape_v = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimReshape>);
  MS_CHECK_TRUE_RET(is_reshape_v != nullptr, {});
  auto reshape_v = VectorRef({is_reshape_v, reshape_v_input_1, reshape_v_input_2});
  // matmul
  auto is_matmul_2 = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimMatMulFusion>);
  MS_CHECK_TRUE_RET(is_matmul_2 != nullptr, {});
  auto matmul_2 = VectorRef({is_matmul_2, cast, reshape_v});
  // output reshape to four dims
  auto reshape_o_2 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(reshape_o_2 != nullptr, {});
  auto is_reshape_o = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimReshape>);
  MS_CHECK_TRUE_RET(is_reshape_o != nullptr, {});
  auto reshape_o = VectorRef({is_reshape_o, matmul_2, reshape_o_2});
  return reshape_o;
}

const VectorRef FlashAttentionFusion::DefineFlashAttentionPatternForSDBSH() const {
  // Q: three dim input reshape to four dims
  auto input_q_reshape_param_1 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(input_q_reshape_param_1 != nullptr, {});
  auto input_q_reshape_param_2 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(input_q_reshape_param_2 != nullptr, {});
  auto is_input_q_reshape = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimReshape>);
  MS_CHECK_TRUE_RET(is_input_q_reshape != nullptr, {});
  auto input_q_reshape = VectorRef({is_input_q_reshape, input_q_reshape_param_1, input_q_reshape_param_2});
  //  transpose
  auto is_input_q_transpose_param = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(is_input_q_transpose_param != nullptr, {});
  auto is_input_q_transpose = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimTranspose>);
  MS_CHECK_TRUE_RET(is_input_q_transpose != nullptr, {});
  auto input_q_transpose = VectorRef({is_input_q_transpose, input_q_reshape, is_input_q_transpose_param});
  // Q reshape
  auto reshape_q_input_2 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(reshape_q_input_2 != nullptr, {});
  auto is_reshape_q = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimReshape>);
  MS_CHECK_TRUE_RET(is_reshape_q != nullptr, {});
  auto reshape_q = VectorRef({is_reshape_q, input_q_transpose, reshape_q_input_2});

  // K: three dim input reshape to four dims
  auto input_k_reshape_param_1 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(input_k_reshape_param_1 != nullptr, {});
  auto input_k_reshape_param_2 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(input_k_reshape_param_2 != nullptr, {});
  auto is_input_k_reshape = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimReshape>);
  MS_CHECK_TRUE_RET(is_input_k_reshape != nullptr, {});
  auto input_k_reshape = VectorRef({is_input_k_reshape, input_k_reshape_param_1, input_k_reshape_param_2});
  //  transpose
  auto is_input_k_transpose_param = std::make_shared<Var>();
  auto is_input_k_transpose = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimTranspose>);
  auto input_k_transpose = VectorRef({is_input_k_transpose, input_k_reshape, is_input_k_transpose_param});
  // K reshape
  auto reshape_k_input_2 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(reshape_k_input_2 != nullptr, {});
  auto is_reshape_k = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimReshape>);
  MS_CHECK_TRUE_RET(is_reshape_k != nullptr, {});
  auto reshape_k = VectorRef({is_reshape_k, input_k_transpose, reshape_k_input_2});
  // transpose
  auto is_transpose_param = std::make_shared<CondVar>(IsParamNode);
  MS_CHECK_TRUE_RET(is_transpose_param != nullptr, {});
  auto is_transpose = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimTranspose>);
  MS_CHECK_TRUE_RET(is_transpose != nullptr, {});
  auto transpose = VectorRef({is_transpose, reshape_k, is_transpose_param});
  // matmul 1
  auto is_matmul_1 = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimMatMulFusion>);
  MS_CHECK_TRUE_RET(is_matmul_1 != nullptr, {});
  auto matmul_1 = VectorRef({is_matmul_1, reshape_q, transpose});
  // mul
  auto is_mul_param = std::make_shared<CondVar>(IsParamNode);
  MS_CHECK_TRUE_RET(is_mul_param != nullptr, {});
  auto is_mul = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimMulFusion>);
  MS_CHECK_TRUE_RET(is_mul != nullptr, {});
  auto mul = VectorRef({is_mul, matmul_1, is_mul_param});
  // softmax
  auto is_softmax = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimSoftmax>);
  MS_CHECK_TRUE_RET(is_softmax != nullptr, {});
  auto softmax = VectorRef({is_softmax, mul});
  // cast
  auto is_cast_param = std::make_shared<CondVar>(IsParamNode);
  MS_CHECK_TRUE_RET(is_cast_param != nullptr, {});
  auto is_cast = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimCast>);
  MS_CHECK_TRUE_RET(is_cast != nullptr, {});
  auto cast = VectorRef({is_cast, softmax, is_cast_param});

  // V: three dim input reshape to four dims
  auto input_v_reshape_param_1 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(input_v_reshape_param_1 != nullptr, {});
  auto input_v_reshape_param_2 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(input_v_reshape_param_2 != nullptr, {});
  auto is_input_v_reshape = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimReshape>);
  MS_CHECK_TRUE_RET(is_input_v_reshape != nullptr, {});
  auto input_v_reshape = VectorRef({is_input_v_reshape, input_v_reshape_param_1, input_v_reshape_param_2});
  //  transpose
  auto is_input_v_transpose_param = std::make_shared<CondVar>(IsParamNode);
  MS_CHECK_TRUE_RET(is_input_v_transpose_param != nullptr, {});
  auto is_input_v_transpose = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimTranspose>);
  MS_CHECK_TRUE_RET(is_input_v_transpose != nullptr, {});
  auto input_v_transpose = VectorRef({is_input_v_transpose, input_v_reshape, is_input_v_transpose_param});
  // V reshape
  auto reshape_v_input_2 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(reshape_v_input_2 != nullptr, {});
  auto is_reshape_v = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimReshape>);
  MS_CHECK_TRUE_RET(is_reshape_v != nullptr, {});
  auto reshape_v = VectorRef({is_reshape_v, input_v_transpose, reshape_v_input_2});
  // matmul
  auto is_matmul_2 = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimMatMulFusion>);
  MS_CHECK_TRUE_RET(is_matmul_2 != nullptr, {});
  auto matmul_2 = VectorRef({is_matmul_2, cast, reshape_v});
  // output reshape to four dims
  auto reshape_o_2 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(reshape_o_2 != nullptr, {});
  auto is_reshape_o = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimReshape>);
  MS_CHECK_TRUE_RET(is_reshape_o != nullptr, {});
  auto reshape_o = VectorRef({is_reshape_o, matmul_2, reshape_o_2});
  // output transpose
  auto is_transpose_o_param = std::make_shared<CondVar>(IsParamNode);
  MS_CHECK_TRUE_RET(is_transpose_o_param != nullptr, {});
  auto is_transpose_o = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimTranspose>);
  MS_CHECK_TRUE_RET(is_transpose_o != nullptr, {});
  auto transpose_o = VectorRef({is_transpose_o, reshape_o, is_transpose_o_param});
  // output reshape to three dims
  auto reshape_o2_2 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(reshape_o2_2 != nullptr, {});
  auto is_reshape_o2 = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimReshape>);
  MS_CHECK_TRUE_RET(is_reshape_o2 != nullptr, {});
  auto reshape_o2 = VectorRef({is_reshape_o2, transpose_o, reshape_o2_2});
  return reshape_o2;
}

const VectorRef FlashAttentionFusion::DefineFlashAttentionPatternForPg() const {
  // Q reshape
  auto reshape_q_input_1 = std::make_shared<Var>();  // input Q
  MS_CHECK_TRUE_RET(reshape_q_input_1 != nullptr, {});
  auto reshape_q_input_2 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(reshape_q_input_2 != nullptr, {});
  auto is_reshape_q = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimReshape>);
  MS_CHECK_TRUE_RET(is_reshape_q != nullptr, {});
  auto reshape_q = VectorRef({is_reshape_q, reshape_q_input_1, reshape_q_input_2});
  // Q transpose
  auto is_transpose_q_param = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(is_transpose_q_param != nullptr, {});
  auto is_transpose_q = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimTranspose>);
  MS_CHECK_TRUE_RET(is_transpose_q != nullptr, {});
  auto transpose_q = VectorRef({is_transpose_q, reshape_q, is_transpose_q_param});
  // q div
  auto is_div_q_param = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(is_div_q_param != nullptr, {});
  auto is_div_q = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimRealDiv>);
  MS_CHECK_TRUE_RET(is_div_q != nullptr, {});
  auto div_q = VectorRef({is_div_q, transpose_q, is_div_q_param});
  // K reshape
  auto reshape_k_input_1 = std::make_shared<Var>();  // input K
  MS_CHECK_TRUE_RET(reshape_k_input_1 != nullptr, {});
  auto reshape_k_input_2 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(reshape_k_input_2 != nullptr, {});
  auto is_reshape_k = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimReshape>);
  MS_CHECK_TRUE_RET(is_reshape_k != nullptr, {});
  auto reshape_k = VectorRef({is_reshape_k, reshape_k_input_1, reshape_k_input_2});
  // K transpose
  auto is_transpose_k_param = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(is_transpose_k_param != nullptr, {});
  auto is_transpose_k = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimTranspose>);
  MS_CHECK_TRUE_RET(is_transpose_k != nullptr, {});
  auto transpose_k = VectorRef({is_transpose_k, reshape_k, is_transpose_k_param});
  // matmul 1
  auto is_matmul_1 = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimBatchMatMul>);
  MS_CHECK_TRUE_RET(is_matmul_1 != nullptr, {});
  auto matmul_1 = VectorRef({is_matmul_1, div_q, transpose_k});
  // cast 1
  auto is_cast_1_param = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(is_cast_1_param != nullptr, {});
  auto is_cast_1 = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimCast>);
  MS_CHECK_TRUE_RET(is_cast_1 != nullptr, {});
  auto cast_1 = VectorRef({is_cast_1, matmul_1, is_cast_1_param});
  // ===== attention mask =====
  // sub
  auto sub_mask_input_1 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(sub_mask_input_1 != nullptr, {});
  auto sub_mask_input_2 = std::make_shared<Var>();  // input attention mask
  MS_CHECK_TRUE_RET(sub_mask_input_2 != nullptr, {});
  auto is_mask_sub = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimSub>);
  MS_CHECK_TRUE_RET(is_mask_sub != nullptr, {});
  auto mask_sub = VectorRef({is_mask_sub, sub_mask_input_1, sub_mask_input_2});
  // mul
  auto is_mask_mul_param = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(is_mask_mul_param != nullptr, {});
  auto is_mask_mul = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimMul>);
  MS_CHECK_TRUE_RET(is_mask_mul != nullptr, {});
  auto mask_mul = VectorRef({is_mask_mul, mask_sub, is_mask_mul_param});
  // ===== end attention mask =====
  // add
  auto is_add = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimAdd>);
  MS_CHECK_TRUE_RET(is_add != nullptr, {});
  auto add = VectorRef({is_add, mask_mul, cast_1});
  // softmax
  auto is_softmax = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimSoftmax>);
  MS_CHECK_TRUE_RET(is_softmax != nullptr, {});
  auto softmax = VectorRef({is_softmax, add});
  // cast 2
  auto is_cast_2_param = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(is_cast_2_param != nullptr, {});
  auto is_cast_2 = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimCast>);
  MS_CHECK_TRUE_RET(is_cast_2 != nullptr, {});
  auto cast_2 = VectorRef({is_cast_2, softmax, is_cast_2_param});
  // V reshape
  auto reshape_v_input_1 = std::make_shared<Var>();  // input V
  MS_CHECK_TRUE_RET(reshape_v_input_1 != nullptr, {});
  auto reshape_v_input_2 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(reshape_v_input_2 != nullptr, {});
  auto is_reshape_v = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimReshape>);
  MS_CHECK_TRUE_RET(is_reshape_v != nullptr, {});
  auto reshape_v = VectorRef({is_reshape_v, reshape_v_input_1, reshape_v_input_2});
  // V transpose
  auto is_transpose_v_param = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(is_transpose_v_param != nullptr, {});
  auto is_transpose_v = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimTranspose>);
  MS_CHECK_TRUE_RET(is_transpose_v != nullptr, {});
  auto transpose_v = VectorRef({is_transpose_v, reshape_v, is_transpose_v_param});
  // matmul 2
  auto is_matmul_2 = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimBatchMatMul>);
  MS_CHECK_TRUE_RET(is_matmul_2 != nullptr, {});
  auto matmul_2 = VectorRef({is_matmul_2, cast_2, transpose_v});
  return matmul_2;
}

const VectorRef FlashAttentionFusion::DefineFlashAttentionPatternForLLAMAPatternV1() const {
  // matmul 1
  auto matmul_1_q_input = std::make_shared<Var>();  // input Q
  MS_CHECK_TRUE_RET(matmul_1_q_input != nullptr, {});
  auto matmul_1_k_input = std::make_shared<Var>();  // input K
  MS_CHECK_TRUE_RET(matmul_1_k_input != nullptr, {});
  auto is_matmul_1 = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimBatchMatMul>);
  MS_CHECK_TRUE_RET(is_matmul_1 != nullptr, {});
  auto matmul_1 = VectorRef({is_matmul_1, matmul_1_q_input, matmul_1_k_input});
  // mul
  auto is_mul_param = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(is_mul_param != nullptr, {});
  auto is_mul = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimMul>);
  MS_CHECK_TRUE_RET(is_mul != nullptr, {});
  auto mul = VectorRef({is_mul, matmul_1, is_mul_param});
  // ===== attention mask =====
  // sub
  auto sub_mask_input_1 = std::make_shared<Var>();  // input attention mask
  MS_CHECK_TRUE_RET(sub_mask_input_1 != nullptr, {});
  auto sub_mask_input_2 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(sub_mask_input_2 != nullptr, {});
  auto is_mask_sub = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimSub>);
  MS_CHECK_TRUE_RET(is_mask_sub != nullptr, {});
  auto mask_sub = VectorRef({is_mask_sub, sub_mask_input_1, sub_mask_input_2});
  // mul
  auto is_mask_mul_param = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(is_mask_mul_param != nullptr, {});
  auto is_mask_mul = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimMul>);
  MS_CHECK_TRUE_RET(is_mask_mul != nullptr, {});
  auto mask_mul = VectorRef({is_mask_mul, mask_sub, is_mask_mul_param});
  // ===== end attention mask =====
  // add
  auto is_add = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimAdd>);
  MS_CHECK_TRUE_RET(is_add != nullptr, {});
  auto add = VectorRef({is_add, mask_mul, mul});
  // cast 1
  auto is_cast_1_param = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(is_cast_1_param != nullptr, {});
  auto is_cast_1 = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimCast>);
  MS_CHECK_TRUE_RET(is_cast_1 != nullptr, {});
  auto cast_1 = VectorRef({is_cast_1, add, is_cast_1_param});
  // softmax
  auto is_softmax = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimSoftmax>);
  MS_CHECK_TRUE_RET(is_softmax != nullptr, {});
  auto softmax = VectorRef({is_softmax, cast_1});
  // cast 2
  auto is_cast_2_param = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(is_cast_2_param != nullptr, {});
  auto is_cast_2 = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimCast>);
  MS_CHECK_TRUE_RET(is_cast_2 != nullptr, {});
  auto cast_2 = VectorRef({is_cast_2, softmax, is_cast_2_param});
  // matmul
  auto v_input = std::make_shared<Var>();  // input V
  MS_CHECK_TRUE_RET(v_input != nullptr, {});
  auto is_matmul_2 = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimBatchMatMul>);
  MS_CHECK_TRUE_RET(is_matmul_2 != nullptr, {});
  auto matmul_2 = VectorRef({is_matmul_2, cast_2, v_input});
  return matmul_2;
}

const VectorRef FlashAttentionFusion::DefineFlashAttentionPatternForLLAMAPatternV2() const {
  // matmul 1
  auto matmul_1_q_input = std::make_shared<Var>();  // input Q
  MS_CHECK_TRUE_RET(matmul_1_q_input != nullptr, {});
  auto matmul_1_k_input = std::make_shared<Var>();  // input K
  MS_CHECK_TRUE_RET(matmul_1_k_input != nullptr, {});
  auto is_matmul_1 = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimBatchMatMul>);
  MS_CHECK_TRUE_RET(is_matmul_1 != nullptr, {});
  auto matmul_1 = VectorRef({is_matmul_1, matmul_1_q_input, matmul_1_k_input});
  // mul
  auto is_mul_param = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(is_mul_param != nullptr, {});
  auto is_mul = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimMul>);
  MS_CHECK_TRUE_RET(is_mul != nullptr, {});
  auto mul = VectorRef({is_mul, matmul_1, is_mul_param});
  // ===== attention mask =====
  // sub
  auto sub_mask_input_1 = std::make_shared<Var>();  // input attention mask
  MS_CHECK_TRUE_RET(sub_mask_input_1 != nullptr, {});
  auto sub_mask_input_2 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(sub_mask_input_2 != nullptr, {});
  auto is_mask_sub = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimSub>);
  MS_CHECK_TRUE_RET(is_mask_sub != nullptr, {});
  auto mask_sub = VectorRef({is_mask_sub, sub_mask_input_1, sub_mask_input_2});
  // mul
  auto is_mask_mul_param = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(is_mask_mul_param != nullptr, {});
  auto is_mask_mul = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimMul>);
  MS_CHECK_TRUE_RET(is_mask_mul != nullptr, {});
  auto mask_mul = VectorRef({is_mask_mul, mask_sub, is_mask_mul_param});
  // ===== end attention mask =====
  // add
  auto is_add = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimAdd>);
  MS_CHECK_TRUE_RET(is_add != nullptr, {});
  auto add = VectorRef({is_add, mask_mul, mul});
  // softmax
  auto is_softmax = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimSoftmax>);
  MS_CHECK_TRUE_RET(is_softmax != nullptr, {});
  auto softmax = VectorRef({is_softmax, add});
  // matmul
  auto v_input = std::make_shared<Var>();  // input V
  MS_CHECK_TRUE_RET(v_input != nullptr, {});
  auto is_matmul_2 = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimBatchMatMul>);
  MS_CHECK_TRUE_RET(is_matmul_2 != nullptr, {});
  auto matmul_2 = VectorRef({is_matmul_2, softmax, v_input});
  return matmul_2;
}

CNodePtr FlashAttentionFusion::CreatePromptFlashAttentionCnodeForBNSD(const FuncGraphPtr &func_graph,
                                                                      const AnfNodePtr &node, const AnfNodePtr &q,
                                                                      const AnfNodePtr &k, const AnfNodePtr &v,
                                                                      const AnfNodePtr &atten_mask, int64_t num_heads,
                                                                      int64_t next_token, float scale_value) const {
  MS_LOG(INFO) << "CreatePromptFlashAttentionCnodeForBNSD";
  // create op
  auto prompt_flash_attention_prim = std::make_shared<ops::PromptFlashAttention>();
  if (prompt_flash_attention_prim == nullptr) {
    MS_LOG(ERROR) << "new prompt flash attention prim failed.";
    return nullptr;
  }
  // add attr
  prompt_flash_attention_prim->AddAttr("num_heads", api::MakeValue(num_heads));
  prompt_flash_attention_prim->AddAttr("input_layout", api::MakeValue("BNSD"));
  prompt_flash_attention_prim->AddAttr("next_tokens", api::MakeValue(next_token));
  prompt_flash_attention_prim->AddAttr("scale_value", api::MakeValue(scale_value));

  MS_LOG(INFO) << "q name: " << q->fullname_with_scope() << ", k name: " << k->fullname_with_scope()
               << ", v name: " << v->fullname_with_scope();
  MS_LOG(INFO) << "num heads: " << num_heads << ", input layout: BNSD, next tokens: " << next_token
               << ", scale value: " << scale_value;
  auto fa_prim_c = prompt_flash_attention_prim->GetPrim();
  if (fa_prim_c == nullptr) {
    MS_LOG(ERROR) << "fa_prim_c is nullptr.";
    return nullptr;
  }
  CNodePtr prompt_flash_attention_cnode = nullptr;
  if (atten_mask != nullptr) {
    prompt_flash_attention_cnode = func_graph->NewCNode(fa_prim_c, {q, k, v, atten_mask});
  } else {
    prompt_flash_attention_cnode = func_graph->NewCNode(fa_prim_c, {q, k, v});
  }
  if (prompt_flash_attention_cnode == nullptr) {
    MS_LOG(ERROR) << "new cnode failed.";
    return nullptr;
  }
  prompt_flash_attention_cnode->set_fullname_with_scope(node->fullname_with_scope() + "_prompt_flash_attention_bnsd");
  if (node->abstract() != nullptr) {
    prompt_flash_attention_cnode->set_abstract(node->abstract()->Clone());
  }
  MS_LOG(INFO) << "create PromptFlashAttention success.";
  return prompt_flash_attention_cnode;
}

CNodePtr FlashAttentionFusion::CreatePromptFlashAttentionCnodeForBSH(const FuncGraphPtr &func_graph,
                                                                     const AnfNodePtr &node, const AnfNodePtr &q,
                                                                     const AnfNodePtr &k, const AnfNodePtr &v,
                                                                     const AnfNodePtr &atten_mask, int64_t num_heads,
                                                                     int64_t next_token, float scale_value) const {
  MS_LOG(INFO) << "CreatePromptFlashAttentionCnodeForBSH";
  MS_LOG(INFO) << "input Q name: " << q->fullname_with_scope() << " ,input K name: " << k->fullname_with_scope()
               << " ,input V name: " << v->fullname_with_scope();
  // create op
  auto prompt_flash_attention_prim = std::make_shared<ops::PromptFlashAttention>();
  if (prompt_flash_attention_prim == nullptr) {
    MS_LOG(ERROR) << "incre_flash_attention_prim is nullptr.";
    return nullptr;
  }
  // add attr
  prompt_flash_attention_prim->AddAttr("num_heads", api::MakeValue(num_heads));
  prompt_flash_attention_prim->AddAttr("input_layout", api::MakeValue("BSH"));
  prompt_flash_attention_prim->AddAttr("next_tokens", api::MakeValue(next_token));
  prompt_flash_attention_prim->AddAttr("scale_value", api::MakeValue(scale_value));

  MS_LOG(INFO) << "num heads: " << num_heads << ", input layout: BSH, next tokens: " << next_token
               << ", scale value: " << scale_value;
  auto fa_prim_c = prompt_flash_attention_prim->GetPrim();
  CNodePtr prompt_flash_attention_cnode = nullptr;
  if (atten_mask != nullptr) {
    prompt_flash_attention_cnode = func_graph->NewCNode(fa_prim_c, {q, k, v, atten_mask});
  } else {
    prompt_flash_attention_cnode = func_graph->NewCNode(fa_prim_c, {q, k, v});
  }
  if (prompt_flash_attention_cnode == nullptr) {
    MS_LOG(ERROR) << "new cnode failed.";
    return nullptr;
  }
  prompt_flash_attention_cnode->set_fullname_with_scope(node->fullname_with_scope() + "_prompt_flash_attention_bsh");
  if (node->abstract() != nullptr) {
    prompt_flash_attention_cnode->set_abstract(node->abstract()->Clone());
  }
  MS_LOG(INFO) << "create PromptFlashAttention success.";
  return prompt_flash_attention_cnode;
}

CNodePtr FlashAttentionFusion::CreateIncreFlashAttentionCnodeForBNSD(const FuncGraphPtr &func_graph,
                                                                     const AnfNodePtr &node, const AnfNodePtr &q,
                                                                     const AnfNodePtr &k, const AnfNodePtr &v,
                                                                     const AnfNodePtr &atten_mask, int64_t num_heads,
                                                                     int64_t next_token, float scale_value) const {
  MS_LOG(INFO) << "CreateIncreFlashAttentionCnodeForBNSD";
  // create op
  auto incre_flash_attention_prim = std::make_shared<ops::IncreFlashAttention>();
  if (incre_flash_attention_prim == nullptr) {
    MS_LOG(ERROR) << "incre_flash_attention_prim is nullptr.";
    return nullptr;
  }
  // add attr
  incre_flash_attention_prim->AddAttr("num_heads", api::MakeValue(num_heads));
  incre_flash_attention_prim->AddAttr("input_layout", api::MakeValue("BNSD"));
  incre_flash_attention_prim->AddAttr("next_tokens", api::MakeValue(next_token));
  incre_flash_attention_prim->AddAttr("scale_value", api::MakeValue(scale_value));

  MS_LOG(INFO) << "num heads: " << num_heads << ", input layout: BNSD, next tokens: " << next_token
               << ", scale value: " << scale_value;
  auto fa_prim_c = incre_flash_attention_prim->GetPrim();
  CNodePtr incre_flash_attention_cnode = nullptr;
  if (atten_mask != nullptr) {
    incre_flash_attention_cnode = func_graph->NewCNode(fa_prim_c, {q, k, v, atten_mask});
  } else {
    incre_flash_attention_cnode = func_graph->NewCNode(fa_prim_c, {q, k, v});
  }
  if (incre_flash_attention_cnode == nullptr) {
    MS_LOG(ERROR) << "new cnode failed.";
    return nullptr;
  }
  incre_flash_attention_cnode->set_fullname_with_scope(node->fullname_with_scope() + "_incre_flash_attention");
  if (node->abstract() != nullptr) {
    incre_flash_attention_cnode->set_abstract(node->abstract()->Clone());
  }
  MS_LOG(INFO) << "create IncreFlashAttention success.";
  return incre_flash_attention_cnode;
}

CNodePtr FlashAttentionFusion::CreateFlashAttentionNodeForSD(const std::string &pattern_name,
                                                             const FuncGraphPtr &func_graph, const AnfNodePtr &node,
                                                             const EquivPtr &equiv) const {
  auto cnode = node->cast<CNodePtr>();
  if (pattern_name == kNameFlashAttentionPatternForSDBSH) {
    auto reshape_o2 = cnode;
    MS_CHECK_TRUE_RET(reshape_o2 != nullptr, nullptr);
    auto output_trans = reshape_o2->input(kNumIndex1)->cast<CNodePtr>();
    MS_CHECK_TRUE_RET(output_trans != nullptr, nullptr);
    cnode = output_trans->input(kNumIndex1)->cast<CNodePtr>();  // reshape
    MS_CHECK_TRUE_RET(cnode != nullptr, nullptr);
  }
  auto matmul_2 = cnode->input(1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(matmul_2 != nullptr, nullptr);
  auto cast_2 = matmul_2->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(cast_2 != nullptr, nullptr);
  auto softmax = cast_2->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(softmax != nullptr, nullptr);
  auto mul = softmax->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(mul != nullptr, nullptr);
  auto matmul_1 = mul->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(matmul_1 != nullptr, nullptr);
  auto transpose = matmul_1->input(kNumIndex2)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(transpose != nullptr, nullptr);
  auto q_reshape = matmul_1->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(q_reshape != nullptr, nullptr);
  auto k_reshape = transpose->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(k_reshape != nullptr, nullptr);
  auto v_reshape = matmul_2->input(kNumIndex2)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(v_reshape != nullptr, nullptr);

  auto q_trans = q_reshape->input(kNumIndex1);
  MS_CHECK_TRUE_RET(q_trans != nullptr, nullptr);
  auto k_trans = k_reshape->input(kNumIndex1);
  MS_CHECK_TRUE_RET(k_trans != nullptr, nullptr);
  auto v_trans = v_reshape->input(kNumIndex1);
  MS_CHECK_TRUE_RET(v_trans != nullptr, nullptr);

  auto input_tensor_q_shape = GetTensorShape(q_reshape, kNumIndex1);
  auto input_tensor_k_shape = GetTensorShape(k_reshape, kNumIndex1);
  auto input_tensor_v_shape = GetTensorShape(v_reshape, kNumIndex1);

  float scale_value = 0;
  int64_t num_head = 0;
  int64_t next_tokens = kNumMaxNextTokenSize;
  int64_t q_seq_len = 0;  // if seq len is 1, will use IncreFlashAttention
  int64_t k_seq_len = 0;
  int64_t v_seq_len = 0;
  int64_t d_value = 0;
  auto mul_const_input = mul->input(2);
  if (input_tensor_q_shape.size() != kNumShapeSize4 && utils::isa<ParameterPtr>(mul_const_input)) {
    // for dynamic shape: get scale value
    auto mul_param = mul_const_input->cast<ParameterPtr>()->default_param();
    if (mul_param == nullptr) {
      return nullptr;
    }
    auto mul_value = std::dynamic_pointer_cast<tensor::Tensor>(mul_param);
    if (mul_value == nullptr) {
      return nullptr;
    }
    auto mul_data = reinterpret_cast<float *>(mul_value->data_c());
    if (mul_data == nullptr) {
      return nullptr;
    }
    scale_value = mul_data[0];
  } else if (input_tensor_q_shape.size() == kNumShapeSize4) {
    // for static shape: get scale value
    scale_value = 1 / (pow(input_tensor_q_shape[kNumIndex3], 0.5));
    num_head = input_tensor_q_shape[kNumIndex1];
    q_seq_len = input_tensor_q_shape[kNumIndex2];
    k_seq_len = input_tensor_k_shape[kNumIndex2];
    v_seq_len = input_tensor_v_shape[kNumIndex2];
    d_value = input_tensor_q_shape[kNumIndex3];
  } else {
    MS_LOG(ERROR) << "need check Q input tensor shape: " << input_tensor_q_shape;
    return nullptr;
  }
  if (!PFACheckShape(scale_value, q_seq_len, k_seq_len, v_seq_len, d_value)) {
    return nullptr;
  }
  auto q_trans_reshape = q_trans->cast<CNodePtr>()->input(kNumIndex1);
  auto q_tran_reshape_input = q_trans_reshape->cast<CNodePtr>()->input(kNumIndex1);
  auto k_trans_reshape = k_trans->cast<CNodePtr>()->input(kNumIndex1);
  auto k_trans_reshape_input = k_trans_reshape->cast<CNodePtr>()->input(kNumIndex1);
  auto v_trans_reshape = v_trans->cast<CNodePtr>()->input(kNumIndex1);
  auto v_trans_reshape_input = v_trans_reshape->cast<CNodePtr>()->input(kNumIndex1);

  if (input_tensor_q_shape.size() != kNumShapeSize4) {
    num_head = GetNumHeadForSD(q_trans_reshape);
    if (num_head == -1) {
      return nullptr;
    }
  }
  if (q_seq_len != 1 && pattern_name == kNameFlashAttentionPatternForSDBNSD) {
    return CreatePromptFlashAttentionCnodeForBNSD(func_graph, node, q_trans, k_trans, v_trans, nullptr, num_head,
                                                  next_tokens, scale_value);
  }
  if (q_seq_len != 1 && pattern_name == kNameFlashAttentionPatternForSDBSH) {
    return CreatePromptFlashAttentionCnodeForBSH(func_graph, node, q_tran_reshape_input, k_trans_reshape_input,
                                                 v_trans_reshape_input, nullptr, num_head, next_tokens, scale_value);
  }
  return nullptr;
}

CNodePtr FlashAttentionFusion::CreateFlashAttentionNodeForPg(const std::string &pattern_name,
                                                             const FuncGraphPtr &func_graph, const AnfNodePtr &node,
                                                             const EquivPtr &equiv) const {
  auto matmul_2 = node->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(matmul_2 != nullptr, nullptr);
  auto cast_2 = matmul_2->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(cast_2 != nullptr, nullptr);
  auto softmax = cast_2->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(softmax != nullptr, nullptr);
  auto add = softmax->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(add != nullptr, nullptr);
  auto atten_mask_mul = add->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(atten_mask_mul != nullptr, nullptr);
  auto cast_1 = add->input(kNumIndex2)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(cast_1 != nullptr, nullptr);
  auto matmul_1 = cast_1->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(matmul_1 != nullptr, nullptr);
  auto div = matmul_1->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(div != nullptr, nullptr);

  // PromptFlashAttention input tensor
  auto q = div->input(kNumIndex1);
  MS_CHECK_TRUE_RET(q != nullptr, nullptr);
  auto k = matmul_1->input(kNumIndex2);
  MS_CHECK_TRUE_RET(k != nullptr, nullptr);
  auto v = matmul_2->input(kNumIndex2);
  MS_CHECK_TRUE_RET(v != nullptr, nullptr);
  auto atten_mask = atten_mask_mul->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(atten_mask != nullptr, nullptr);

  auto input_tensor_q_shape = GetTensorShape(div, kNumIndex1);
  if (input_tensor_q_shape.size() != kNumDimSize4) {
    MS_LOG(ERROR) << "q shape is not 4 dims";
    return nullptr;
  }
  auto input_tensor_k_shape = GetTensorShape(matmul_1, kNumIndex2);
  if (input_tensor_k_shape.size() != kNumDimSize4) {
    MS_LOG(ERROR) << "k shape is not 4 dims";
    return nullptr;
  }
  auto input_tensor_v_shape = GetTensorShape(matmul_2, kNumIndex2);
  if (input_tensor_v_shape.size() != kNumDimSize4) {
    MS_LOG(ERROR) << "v shape is not 4 dims";
    return nullptr;
  }
  MS_LOG(INFO) << "q name: " << q->fullname_with_scope() << " , k name: " << k->fullname_with_scope()
               << " , v name: " << v->fullname_with_scope();
  MS_LOG(INFO) << "q shape: " << input_tensor_q_shape << ", k shape: " << input_tensor_k_shape
               << ", v shape: " << input_tensor_v_shape;

  // check input shape
  if (input_tensor_q_shape[kNumIndex3] <= 0 || input_tensor_q_shape[kNumIndex1] <= 0) {
    MS_LOG(ERROR) << "D is -1";
    return nullptr;
  }
  float scale_value = 1 / (pow(input_tensor_q_shape[kNumIndex3], 0.5));
  int64_t seq_len = input_tensor_q_shape[kNumIndex2];
  if (seq_len != 1) {
    return CreatePromptFlashAttentionCnodeForBNSD(func_graph, node, q, k, v, atten_mask,
                                                  input_tensor_q_shape[kNumIndex1], 0, scale_value);
  }
  return nullptr;
}

CNodePtr FlashAttentionFusion::CreateFlashAttentionNodeForLLAMAPatternV1(const std::string &pattern_name,
                                                                         const FuncGraphPtr &func_graph,
                                                                         const AnfNodePtr &node,
                                                                         const EquivPtr &equiv) const {
  auto matmul_2 = node->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(matmul_2 != nullptr, nullptr);
  auto cast_2 = matmul_2->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(cast_2 != nullptr, nullptr);
  auto softmax = cast_2->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(softmax != nullptr, nullptr);
  auto cast_1 = softmax->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(cast_1 != nullptr, nullptr);
  auto add = cast_1->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(add != nullptr, nullptr);

  auto attention_mask_mul = add->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(attention_mask_mul != nullptr, nullptr);

  auto mul = add->input(kNumIndex2)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(mul != nullptr, nullptr);
  auto matmul_1 = mul->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(matmul_1 != nullptr, nullptr);
  // PromptFlashAttention input tensor
  auto q = matmul_1->input(kNumIndex1);
  MS_CHECK_TRUE_RET(q != nullptr, nullptr);
  auto k = matmul_1->input(kNumIndex2);
  MS_CHECK_TRUE_RET(k != nullptr, nullptr);
  auto v = matmul_2->input(kNumIndex2);
  MS_CHECK_TRUE_RET(v != nullptr, nullptr);
  auto atten_mask = attention_mask_mul->input(1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(atten_mask != nullptr, nullptr);

  auto input_tensor_q_shape = GetTensorShape(matmul_1, kNumIndex1);
  if (input_tensor_q_shape.size() != kNumDimSize4) {
    MS_LOG(ERROR) << "q shape is not 4 dims";
    return nullptr;
  }
  auto input_tensor_k_shape = GetTensorShape(matmul_1, kNumIndex2);
  if (input_tensor_k_shape.size() != kNumDimSize4) {
    MS_LOG(ERROR) << "k shape is not 4 dims";
    return nullptr;
  }
  auto input_tensor_v_shape = GetTensorShape(matmul_2, kNumIndex2);
  if (input_tensor_v_shape.size() != kNumDimSize4) {
    MS_LOG(ERROR) << "v shape is not 4 dims";
    return nullptr;
  }
  MS_LOG(INFO) << "q name: " << q->fullname_with_scope() << " , k name: " << k->fullname_with_scope()
               << " , v name: " << v->fullname_with_scope();
  MS_LOG(INFO) << "q shape: " << input_tensor_q_shape << ", k shape: " << input_tensor_k_shape
               << ", v shape: " << input_tensor_v_shape;
  // check input shape
  if (input_tensor_q_shape[kNumIndex3] <= 0 || input_tensor_q_shape[kNumIndex1] <= 0) {
    MS_LOG(ERROR) << "D is -1";
    return nullptr;
  }
  float scale_value = 1 / (pow(input_tensor_q_shape[kNumIndex3], 0.5));
  int64_t seq_len = input_tensor_q_shape[kNumIndex2];
  if (seq_len != 1) {
    return CreatePromptFlashAttentionCnodeForBNSD(func_graph, node, q, k, v, atten_mask,
                                                  input_tensor_q_shape[kNumIndex1], 0, scale_value);
  }
  return nullptr;
}

CNodePtr FlashAttentionFusion::CreateFlashAttentionNodeForLLAMAPatternV2(const std::string &pattern_name,
                                                                         const FuncGraphPtr &func_graph,
                                                                         const AnfNodePtr &node,
                                                                         const EquivPtr &equiv) const {
  auto matmul_2 = node->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(matmul_2 != nullptr, nullptr);
  auto softmax = matmul_2->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(softmax != nullptr, nullptr);
  auto add = softmax->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(add != nullptr, nullptr);

  auto attention_mask_mul = add->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(attention_mask_mul != nullptr, nullptr);

  auto mul = add->input(kNumIndex2)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(mul != nullptr, nullptr);
  auto matmul_1 = mul->input(kNumIndex1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(matmul_1 != nullptr, nullptr);
  // PromptFlashAttention input tensor
  auto q = matmul_1->input(kNumIndex1);
  MS_CHECK_TRUE_RET(q != nullptr, nullptr);
  auto k = matmul_1->input(kNumIndex2);
  MS_CHECK_TRUE_RET(k != nullptr, nullptr);
  auto v = matmul_2->input(kNumIndex2);
  MS_CHECK_TRUE_RET(v != nullptr, nullptr);
  auto atten_mask = attention_mask_mul->input(1)->cast<CNodePtr>();
  MS_CHECK_TRUE_RET(atten_mask != nullptr, nullptr);

  auto input_tensor_q_shape = GetTensorShape(matmul_1, kNumIndex1);
  if (input_tensor_q_shape.size() != kNumDimSize4) {
    MS_LOG(ERROR) << "q shape is not 4 dims";
    return nullptr;
  }
  auto input_tensor_k_shape = GetTensorShape(matmul_1, kNumIndex2);
  if (input_tensor_k_shape.size() != kNumDimSize4) {
    MS_LOG(ERROR) << "k shape is not 4 dims";
    return nullptr;
  }
  auto input_tensor_v_shape = GetTensorShape(matmul_2, kNumIndex2);
  if (input_tensor_v_shape.size() != kNumDimSize4) {
    MS_LOG(ERROR) << "v shape is not 4 dims";
    return nullptr;
  }
  MS_LOG(INFO) << "q name: " << q->fullname_with_scope() << " , k name: " << k->fullname_with_scope()
               << " , v name: " << v->fullname_with_scope();
  MS_LOG(INFO) << "q shape: " << input_tensor_q_shape << ", k shape: " << input_tensor_k_shape
               << ", v shape: " << input_tensor_v_shape;
  // check input shape
  if (input_tensor_q_shape[kNumIndex3] <= 0 || input_tensor_q_shape[kNumIndex1] <= 0) {
    MS_LOG(ERROR) << "D is -1";
    return nullptr;
  }
  float scale_value = 1 / (pow(input_tensor_q_shape[kNumIndex3], 0.5));
  int64_t seq_len = input_tensor_q_shape[kNumIndex2];
  if (seq_len != 1) {
    return CreatePromptFlashAttentionCnodeForBNSD(func_graph, node, q, k, v, atten_mask,
                                                  input_tensor_q_shape[kNumIndex1], 0, scale_value);
  }
  return nullptr;
}

AnfNodePtr FlashAttentionFusion::Process(const std::string &patten_name, const FuncGraphPtr &func_graph,
                                         const AnfNodePtr &node, const EquivPtr &equiv) const {
  MS_LOG(INFO) << "do fusion, pattern name: " << patten_name;
  if (func_graph == nullptr || node == nullptr || equiv == nullptr) {
    MS_LOG(ERROR) << "function graph, node or equiv is nullptr.";
    return nullptr;
  }
  if (!utils::isa<CNodePtr>(node)) {
    MS_LOG(ERROR) << "this node is not cnode, node name: " << node->fullname_with_scope();
    return nullptr;
  }
  if (IsMarkedTrainOp(utils::cast<CNodePtr>(node))) {
    MS_LOG(ERROR) << "node is train op, can not fusion.";
    return nullptr;
  }
  auto manager = Manage(func_graph);
  if (manager == nullptr) {
    MS_LOG(ERROR) << "manager is nullptr.";
    return nullptr;
  }
  CNodePtr flash_attention_node = nullptr;
  if (patten_name == kNameFlashAttentionPatternForSDBNSD || patten_name == kNameFlashAttentionPatternForSDBSH) {
    flash_attention_node = CreateFlashAttentionNodeForSD(patten_name, func_graph, node, equiv);
  } else if (patten_name == kNameFlashAttentionPatternForPg) {
    flash_attention_node = CreateFlashAttentionNodeForPg(patten_name, func_graph, node, equiv);
  } else if (patten_name == kNameFlashAttentionPatternForLLAMAPatternV1) {
    flash_attention_node = CreateFlashAttentionNodeForLLAMAPatternV1(patten_name, func_graph, node, equiv);
  } else if (patten_name == kNameFlashAttentionPatternForLLAMAPatternV2) {
    flash_attention_node = CreateFlashAttentionNodeForLLAMAPatternV2(patten_name, func_graph, node, equiv);
  } else {
    MS_LOG(ERROR) << " not patter.";
  }
  if (flash_attention_node == nullptr) {
    MS_LOG(INFO) << "flash attention op not fusion.";
    return nullptr;
  }
  manager->Replace(node, flash_attention_node);
  MS_LOG(INFO) << "flash attention node fusion success, node name: " << flash_attention_node->fullname_with_scope();
  return flash_attention_node;
}

}  // namespace mindspore::opt
