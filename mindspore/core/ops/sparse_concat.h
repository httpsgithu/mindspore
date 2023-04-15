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

#ifndef MINDSPORE_CORE_OPS_SPARSE_CONCAT_H_
#define MINDSPORE_CORE_OPS_SPARSE_CONCAT_H_
#include <memory>
#include <string>
#include <vector>

#include "ops/base_operator.h"
#include "mindapi/base/types.h"

namespace mindspore {
namespace ops {
constexpr auto kNameSparseConcat = "SparseConcat";

class MIND_API SparseConcat : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(SparseConcat);
  /// \brief Constructor.
  SparseConcat() : BaseOperator(kNameSparseConcat) {
    InitIOName(
      {
        "axis",
        "sp_input_indices",
        "sp_input_values",
        "sp_input_shapes",
      },
      {"concat_dim"});
  }
  /// \brief Init.
  /// Refer to the parameters of python API @ref mindspore.ops.sparse_ops.SparseConcat for the more details.
  void Init(int64_t concat_dim);
  /// \brief Set concat_dim.
  void set_concat_dim(const int64_t &concat_dim);
  /// \brief Get concat_dim.
  ///
  /// \return bool concat_dim value.
  int64_t get_concat_dim() const;

 private:
  const std::string kConcatDim = "concat_dim";
};
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_SPARSE_CONCAT_H_
