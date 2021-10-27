/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#include "nnacl/infer/lstm_infer.h"
#include "nnacl/infer/infer_register.h"

int CheckInputShapeValid(const TensorC *const *inputs, const LstmParameter *parameter) {
  const TensorC *input = inputs[FIRST_INPUT];
  const TensorC *weight_i = inputs[SECOND_INPUT];
  const TensorC *weight_g = inputs[THIRD_INPUT];
  const TensorC *bias = inputs[FOURTH_INPUT];
  const TensorC *cell = inputs[FIFTH_INPUT];
  int batch = input->shape_[kNHWC_H];
  int input_size = input->shape_[kNHWC_W];
  int hidden_size = weight_i->shape_[kNHWC_H] / C4NUM;
  bool bidirectional = parameter->bidirectional_;
  if (input->shape_size_ != DIMENSION_3D || weight_i->shape_size_ != DIMENSION_3D) {
    return NNACL_ERR;
  }
  int bidirection = bidirectional ? C2NUM : C1NUM;
  MS_CHECK_TRUE_RET(weight_i->shape_[kNHWC_N] == bidirection && weight_i->shape_[kNHWC_H] == hidden_size * C4NUM &&
                      weight_i->shape_[kNHWC_W] == input_size,
                    NNACL_ERR);
  MS_CHECK_TRUE_RET(weight_g->shape_[kNHWC_N] == bidirection && weight_g->shape_[kNHWC_H] == hidden_size * C4NUM &&
                      weight_g->shape_[kNHWC_W] == hidden_size,
                    NNACL_ERR);
  MS_CHECK_TRUE_RET(bias->shape_[kNHWC_N] == bidirection && bias->shape_[kNHWC_H] == hidden_size * C8NUM, NNACL_ERR);
  if (!bidirectional && cell->shape_size_ == DIMENSION_2D) {
    MS_CHECK_TRUE_RET(cell->shape_[kNHWC_N] == batch && cell->shape_[kNHWC_H] == hidden_size, NNACL_ERR);
  } else {
    MS_CHECK_TRUE_RET(
      cell->shape_[kNHWC_N] == bidirection && cell->shape_[kNHWC_H] == batch && cell->shape_[kNHWC_W] == hidden_size,
      NNACL_ERR);
  }
  return NNACL_OK;
}

int LstmInferShape(const TensorC *const *inputs, size_t inputs_size, TensorC **outputs, size_t outputs_size,
                   OpParameter *parameter) {
  int check_ret = CheckAugmentNullSize(inputs, inputs_size, outputs, outputs_size, parameter, 6, 3);
  if (check_ret != NNACL_OK) {
    return check_ret;
  }

  const TensorC *input = inputs[0];
  const TensorC *weight_i = inputs[1];
  TensorC *output = outputs[0];
  for (int i = 0; i < 3; i++) {
    SetDataTypeFormat(outputs[i], input);
  }

  LstmParameter *param = (LstmParameter *)parameter;
  NNACL_CHECK_NULL_RETURN_ERR(param);
  if (!InferFlag(inputs, inputs_size)) {
    return NNACL_INFER_INVALID;
  }

  if (CheckInputShapeValid(inputs, param) != NNACL_OK) {
    return NNACL_ERR;
  }

  int hidden_size = weight_i->shape_[1] / 4;
  int out_shape[MAX_SHAPE_SIZE];
  size_t out_shape_size = 0;
  ShapeSet(out_shape, &out_shape_size, input->shape_, input->shape_size_);
  out_shape[2] = hidden_size;
  if (param->bidirectional_) {
    int ret = ShapeInsert(out_shape, &out_shape_size, 1, 2);
    if (ret != NNACL_OK) {
      return NNACL_ERR;
    }
  } else {
    int ret = ShapeInsert(out_shape, &out_shape_size, 1, 1);
    if (ret != NNACL_OK) {
      return NNACL_ERR;
    }
  }
  SetShapeArray(output, out_shape, out_shape_size);
  int state_shape[MAX_SHAPE_SIZE];
  size_t state_shape_size = 0;
  ShapeSet(state_shape, &state_shape_size, input->shape_, input->shape_size_);
  state_shape[0] = param->bidirectional_ ? 2 : 1;
  state_shape[2] = hidden_size;
  SetShapeArray(outputs[1], state_shape, state_shape_size);
  SetShapeArray(outputs[2], state_shape, state_shape_size);

  return NNACL_OK;
}

REG_INFER(LSTM, PrimType_LSTM, LstmInferShape)
