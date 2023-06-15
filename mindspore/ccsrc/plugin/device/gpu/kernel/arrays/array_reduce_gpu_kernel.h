/**
 * Copyright 2019-2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_ARRAYS_ARRAY_REDUCE_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_ARRAYS_ARRAY_REDUCE_GPU_KERNEL_H_

#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <utility>
#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/reduce_impl.cuh"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"
#include "plugin/device/gpu/kernel/kernel_constants.h"
#include "utils/check_convert_utils.h"
namespace mindspore {
namespace kernel {
class ArrayReduceGpuKernelMod : public NativeGpuKernelMod {
 public:
  ArrayReduceGpuKernelMod() { ResetResource(); }
  explicit ArrayReduceGpuKernelMod(const std::string &kernel_type) : kernel_type_(kernel_type) { ResetResource(); }
  ~ArrayReduceGpuKernelMod() = default;

  bool Init(const BaseOperatorPtr &base_operator, const std::vector<KernelTensorPtr> &inputs,
            const std::vector<KernelTensorPtr> &outputs) override;

  int Resize(const BaseOperatorPtr &base_operator, const std::vector<KernelTensorPtr> &inputs,
             const std::vector<KernelTensorPtr> &outputs,
             const std::map<uint32_t, tensor::TensorPtr> &inputsOnHost) override;

  bool Launch(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &workspace,
              const std::vector<AddressPtr> &outputs, void *stream_ptr) override {
    return kernel_func_(this, inputs, workspace, outputs, stream_ptr);
  }

 protected:
  void ResetResource() {
    reduce_op_type_ = ReduceSum;
    need_skip_execute_ = false;
    keep_dims_ = false;
    skip_mode_ = false;
    all_match_ = false;
    is_null_input_ = false;
    input_num_ = 1;
    input_size_ = 0;
    output_size_ = 0;
    workspace_size_ = 0;
    kernel_name_ = "ArrayReduce";
    axis_.clear();
    input_reshape_.clear();
    input_shape_.clear();
    reduce_first_axis_ = false;
  }

  void InferArrayReduceType();
  void FormatAxis(const size_t dims, const std::vector<int> &axis, std::vector<bool> *bitmap);
  std::vector<size_t> ToRowReduce();

  void GetTransposePerm(size_t *transpose_perm);

  void GetOriginShape(size_t *origin_shape);
  void SimplyReduce(const ShapeVector input_shape, const std::vector<int> axis);
  void InferInAndOutDesc(const ShapeVector &input_shape, const ShapeVector &output_shape);
  std::vector<KernelAttr> GetOpSupport() override;
  std::vector<size_t> GetLaunchIgnoredInputAddressIdx() const override { return {kIndex1}; }
  template <typename T>
  bool LaunchKernel(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &workspace,
                    const std::vector<AddressPtr> &outputs, void *stream_ptr);
  template <typename T>
  bool LaunchComplexKernel(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &workspace,
                           const std::vector<AddressPtr> &outputs, void *stream_ptr);
  using ReduceFunc =
    std::function<bool(ArrayReduceGpuKernelMod *, const std::vector<kernel::AddressPtr> &,
                       const std::vector<kernel::AddressPtr> &, const std::vector<kernel::AddressPtr> &, void *)>;
  ReduceFunc kernel_func_;
  static std::vector<std::pair<KernelAttr, ReduceFunc>> all_any_list_;
  static std::vector<std::pair<KernelAttr, ReduceFunc>> prod_list_;
  static std::vector<std::pair<KernelAttr, ReduceFunc>> sum_list_;
  static std::vector<std::pair<KernelAttr, ReduceFunc>> max_min_list_;
  static std::vector<std::pair<KernelAttr, ReduceFunc>> mean_list_;
  static std::map<std::string, std::vector<std::pair<KernelAttr, ReduceFunc>>> kernel_attr_list_;

 private:
  ReduceType_t reduce_op_type_;

  std::vector<int> axis_;
  bool keep_dims_;
  bool skip_mode_;
  bool need_skip_execute_;
  bool all_match_;
  bool is_null_input_;
  size_t input_size_;
  size_t output_size_;
  size_t workspace_size_;
  static constexpr size_t kAxisIndex_{1};
  std::string kernel_type_{"Unknown"};
  bool reduce_first_axis_;
  std::vector<size_t> input_reshape_;
  ShapeVector input_shape_;
  size_t input_num_;
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_ARRAYS_ARRAY_REDUCE_GPU_KERNEL_H_
