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

#ifndef NNACL_POW_PARAMETER_H_
#define NNACL_POW_PARAMETER_H_

#include "nnacl/op_base.h"

typedef struct PowQuantArg {
  QuantArg in_args_;
  QuantArg exp_args_;
  QuantArg out_args_;
  int output_activation_min_;
  int output_activation_max_;
} PowQuantArg;

typedef struct PowParameter {
  OpParameter op_parameter_;
  float power_;
  float scale_;
  float shift_;
} PowParameter;

#endif  // NNACL_POW_PARAMETER_H_
