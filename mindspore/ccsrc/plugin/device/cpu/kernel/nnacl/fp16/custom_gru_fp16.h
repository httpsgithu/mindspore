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
#ifndef NNACL_FP16_CUSTOM_GRU_FP16_H_
#define NNACL_FP16_CUSTOM_GRU_FP16_H_
#ifdef ENABLE_ARM64
#include "nnacl/custom_gru_parameter.h"

#ifdef __cplusplus
extern "C" {
#endif
void CustomGruFp16(float16_t *output, const float16_t *input, const float16_t *weight_input,
                   const float16_t *weight_hidden, const float16_t *bias_input, const float16_t *bias_hidden,
                   const float16_t *init_h, float16_t *buffer[4], const CustomGruParameter *gru_param);
#ifdef __cplusplus
}
#endif

#endif
#endif  //  NNACL_FP16_CUSTOM_GRU_FP16_H_
