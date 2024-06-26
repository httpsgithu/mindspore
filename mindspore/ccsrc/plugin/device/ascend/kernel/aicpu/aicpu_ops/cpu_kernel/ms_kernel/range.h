/**
 * Copyright 2024 Huawei Technologies Co., Ltd
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
#ifndef AICPU_OPS_AICPU_RANGE_KERNELS_H_
#define AICPU_OPS_AICPU_RANGE_KERNELS_H_

#include "inc/ms_cpu_kernel.h"
#include <vector>

namespace aicpu {
class RangeKernel : public CpuKernel {
 public:
  RangeKernel() = default;
  uint32_t Compute(CpuKernelContext &ctx);

 private:
  template <typename T>
  uint32_t RangeTask(CpuKernelContext &ctx);
  DataType index_type_;
};
}  // namespace aicpu
#endif  // AICPU_OPS_AICPU_RANGE_KERNELS_H_
