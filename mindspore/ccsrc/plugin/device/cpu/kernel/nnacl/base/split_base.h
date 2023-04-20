/**
 * Copyright 2020 Huawei Technologies Co., Ltd
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

#ifndef NNACL_NNACL_SPLIT_BASE_H_
#define NNACL_NNACL_SPLIT_BASE_H_

#include "nnacl/op_base.h"
#include "nnacl/split_parameter.h"

#ifdef __cplusplus
extern "C" {
#endif
int DoSplit(const void *in_data, void **out_data, const int *input_shape, int offset, int num_unit,
            const SplitParameter *split_param, int data_size);
#ifdef __cplusplus
}
#endif

#endif  // MINDSPORE_NNACL_NNACL_SPLIT_BASE_H_
