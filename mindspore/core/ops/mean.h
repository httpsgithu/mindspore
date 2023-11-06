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

#ifndef MINDSPORE_CORE_OPS_MEAN_H_
#define MINDSPORE_CORE_OPS_MEAN_H_
#include <memory>
#include <string>
#include <vector>

#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameMean = "Mean";
/// \brief Mean the second input tensor from the first input tensor element-wise.
class MIND_API Mean : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(Mean);
  /// \brief Constructor.
  Mean() : BaseOperator(kNameMean) { InitIOName({"input", "dim", "keepdim", "dtype"}, {"output"}); }
  explicit Mean(const std::string k_name) : BaseOperator(k_name) {
    InitIOName({"input", "dim", "keepdim", "dtype"}, {"output"});
  }
  /// \brief Init.
  void Init() const {}
};

MIND_API abstract::AbstractBasePtr MeanInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                             const std::vector<abstract::AbstractBasePtr> &input_args);
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_MEAN_H_
