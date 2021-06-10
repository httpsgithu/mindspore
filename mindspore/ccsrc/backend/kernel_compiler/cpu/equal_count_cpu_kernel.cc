/**
 * Copyright 2019 Huawei Technologies Co., Ltd
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
#include "backend/kernel_compiler/cpu/equal_count_cpu_kernel.h"
#include "runtime/device/cpu/cpu_device_address.h"

namespace mindspore {
namespace kernel {
void EqualCountCPUKernel::InitKernel(const CNodePtr &) {}

bool EqualCountCPUKernel::Launch(const std::vector<kernel::AddressPtr> &inputs, const std::vector<kernel::AddressPtr> &,
                                 const std::vector<kernel::AddressPtr> &outputs) {
  if (inputs.size() < 2 || outputs.empty()) {
    MS_LOG(EXCEPTION) << "Input or output empty!";
  }
  if (inputs[0]->size != inputs[1]->size) {
    MS_LOG(EXCEPTION) << "Input or output size!";
  }
  int count = 0;
  auto left = reinterpret_cast<int *>(inputs[0]->addr);
  auto right = reinterpret_cast<int *>(inputs[1]->addr);
  size_t elem_num = inputs[0]->size / sizeof(int);
  for (size_t i = 0; i < elem_num; i++) {
    if (left[i] == right[i]) {
      count++;
    }
  }
  auto output = reinterpret_cast<int *>(outputs[0]->addr);
  output[0] = count;
  return true;
}
}  // namespace kernel
}  // namespace mindspore
