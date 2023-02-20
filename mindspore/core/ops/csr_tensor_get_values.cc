/**
 * Copyright 2022 Huawei Technologies Co., Ltd
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

#include "ops/csr_tensor_get_values.h"

#include <memory>

#include "abstract/ops/primitive_infer_map.h"
#include "ops/op_utils.h"
#include "ops/primitive_c.h"
#include "abstract/abstract_value.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ops/core_ops.h"
#include "utils/log_adapter.h"
#include "mindapi/src/helper.h"

namespace mindspore {
namespace ops {
abstract::AbstractBasePtr CSRTensorGetValuesInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                                  const std::vector<abstract::AbstractBasePtr> &args_spec_list) {
  auto csr_tensor = InferSparseAttr<abstract::AbstractCSRTensor>(primitive, args_spec_list);
  MS_EXCEPTION_IF_NULL(csr_tensor->values());
  return csr_tensor->values();
}
MIND_API_OPERATOR_IMPL(CSRTensorGetValues, BaseOperator);
REGISTER_PRIMITIVE_EVAL_IMPL(CSRTensorGetValues, prim::kPrimCSRTensorGetValues, CSRTensorGetValuesInfer, nullptr, true);
}  // namespace ops
}  // namespace mindspore
