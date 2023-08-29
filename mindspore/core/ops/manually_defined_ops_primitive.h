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

#ifndef MINDSPORE_CORE_OPS_MANUAL_DEFINED_OPS_DEF_H_
#define MINDSPORE_CORE_OPS_MANUAL_DEFINED_OPS_DEF_H_

#include <memory>
#include "ir/anf.h"
#include "ir/primitive.h"
#include "ops/manually_defined_ops_name.h"

namespace mindspore::prim {
GVAR_DEF(PrimitivePtr, kPrimBatchNorm, std::make_shared<Primitive>(ops::kNameBatchNorm));
}  // namespace mindspore::prim
#endif  // MINDSPORE_CORE_OPS_MANUAL_DEFINED_OPS_DEF_H_
