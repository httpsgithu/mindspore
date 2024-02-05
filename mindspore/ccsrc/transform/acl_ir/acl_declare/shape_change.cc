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

#include "transform/acl_ir/acl_adapter_info.h"

namespace mindspore {
namespace transform {
REGISTER_ACL_OP(BroadcastTo).set_is_const_input();

REGISTER_ACL_OP(ReverseV2).set_is_const_input();

REGISTER_ACL_OP(ViewCopy).set_is_const_input();

REGISTER_ACL_OP(AsStrided).set_is_const_input();

REGISTER_ACL_OP(ArgMin).set_is_const_input();

REGISTER_ACL_OP(ApplyRMSProp).set_is_const_input();

REGISTER_ACL_OP(Transpose).Input(0, {"ND"}).Input(1, {"ND"});
}  // namespace transform
}  // namespace mindspore
