/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_LITE_TOOLS_CONVERTER_MICRO_CODER_OPCODERS_NNACL_FP32_DECONV2D_FP32_CODER_H_
#define MINDSPORE_LITE_TOOLS_CONVERTER_MICRO_CODER_OPCODERS_NNACL_FP32_DECONV2D_FP32_CODER_H_

#include <vector>
#include <string>
#include "nnacl/conv_parameter.h"
#include "coder/opcoders/base/conv2d_base_coder.h"
#include "coder/opcoders/serializers/nnacl_serializer/nnacl_fp32_serializer.h"
#include "nnacl/fp32/deconv_fp32.h"
#include "nnacl/fp32/matmul_fp32.h"

namespace mindspore::lite::micro::nnacl {
class DeConvolutionFP32Coder : public Conv2DBaseCoder {
 public:
  DeConvolutionFP32Coder(const std::vector<Tensor *> &in_tensors, const std::vector<Tensor *> &out_tensors,
                         const LiteGraph::Node *node, size_t node_index, Target target)
      : Conv2DBaseCoder(in_tensors, out_tensors, node, node_index, target) {}

  int Prepare(CoderContext *const context) override;

  int DoCode(CoderContext *const context) override;

  ~DeConvolutionFP32Coder() override = default;

 protected:
  virtual int InitWeightBias(CoderContext *const context);
  virtual void CollectFilesForFunc(CoderContext *const context);
  int Resize();
  virtual int InitRunBuf();
  int InitParam();

  MatMulParameter matmul_param_{};
  size_t pack_output_size_{0};
  size_t tmp_buffer_size_{0};
  size_t pack_input_size_{0};
  size_t bias_data_size_{0};
  size_t pack_weight_size_{0};
  int input_plane_{0};
  int kernel_plane_{0};
  int output_plane_{0};
  void *packed_bias_{nullptr};
  void *packed_weight_{nullptr};
  void *packed_input_{nullptr};
  void *packed_output_{nullptr};
  void *tmp_buffer_{nullptr};
  std::string input_ptr_;
  std::string output_ptr_;
  TypeId data_type_{kNumberTypeFloat32};
};
}  // namespace mindspore::lite::micro::nnacl
#endif  // MINDSPORE_LITE_TOOLS_CONVERTER_MICRO_CODER_OPCODERS_NNACL_FP32_DECONV2D_FP32_CODER_H_
