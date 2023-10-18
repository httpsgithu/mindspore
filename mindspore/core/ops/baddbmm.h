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

#ifndef MINDSPORE_CORE_OPS_BADDBMM_H_
#define MINDSPORE_CORE_OPS_BADDBMM_H_
#include <memory>
#include <vector>
#include "mindapi/base/types.h"
#include "ops/base_operator.h"
#include "ops/mat_mul.h"

namespace mindspore {
namespace ops {
constexpr auto kNameBaddBmm = "BaddBmm";

/// \brief Computes matrix multiplication between two tensors by batch.
/// Refer to Python API @ref mindspore.ops.BaddBmm for more details.
class MIND_API BaddBmm : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(BaddBmm);
  /// \brief Constructor.
  BaddBmm() : BaseOperator(kNameBaddBmm) { InitIOName({"input", "batch1", "batch2", "alpha", "beta"}, {"output"}); }
  /// \brief Init. Refer to the parameters of Python API @ref mindspore.ops.BaddBmm for the inputs.
  void Init() const {}
};
MIND_API abstract::AbstractBasePtr BaddBmmInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                                const std::vector<abstract::AbstractBasePtr> &input_args);
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_BADDBMM_H_
