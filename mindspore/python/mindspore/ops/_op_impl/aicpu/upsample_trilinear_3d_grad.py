# Copyright 2022 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================
"""UpsampleTrilinear3DGrad op"""
from mindspore.ops.op_info_register import op_info_register, AiCPURegOp, DataType

upsample_trilinear_3d_grad_op_info = AiCPURegOp("UpsampleTrilinear3DGrad") \
    .fusion_type("OPAQUE") \
    .attr("align_corners", "bool") \
    .attr("none_list", "listInt") \
    .input(0, "grad_output", "required") \
    .input(1, "input_size", "required") \
    .input(2, "output_size_or_scales", "required") \
    .output(0, "y", "required") \
    .dtype_format(DataType.F16_Default, DataType.I32_Default, DataType.I32_Default, DataType.F16_Default) \
    .dtype_format(DataType.F32_Default, DataType.I32_Default, DataType.I32_Default, DataType.F32_Default) \
    .dtype_format(DataType.F64_Default, DataType.I32_Default, DataType.I32_Default, DataType.F64_Default) \
    .dtype_format(DataType.F16_Default, DataType.I64_Default, DataType.I32_Default, DataType.F16_Default) \
    .dtype_format(DataType.F32_Default, DataType.I64_Default, DataType.I32_Default, DataType.F32_Default) \
    .dtype_format(DataType.F64_Default, DataType.I64_Default, DataType.I32_Default, DataType.F64_Default) \
    .dtype_format(DataType.F16_Default, DataType.I32_Default, DataType.I64_Default, DataType.F16_Default) \
    .dtype_format(DataType.F32_Default, DataType.I32_Default, DataType.I64_Default, DataType.F32_Default) \
    .dtype_format(DataType.F64_Default, DataType.I32_Default, DataType.I64_Default, DataType.F64_Default) \
    .dtype_format(DataType.F16_Default, DataType.I64_Default, DataType.I64_Default, DataType.F16_Default) \
    .dtype_format(DataType.F32_Default, DataType.I64_Default, DataType.I64_Default, DataType.F32_Default) \
    .dtype_format(DataType.F64_Default, DataType.I64_Default, DataType.I64_Default, DataType.F64_Default) \
    .dtype_format(DataType.F16_Default, DataType.I32_Default, DataType.F32_Default, DataType.F16_Default) \
    .dtype_format(DataType.F32_Default, DataType.I32_Default, DataType.F32_Default, DataType.F32_Default) \
    .dtype_format(DataType.F64_Default, DataType.I32_Default, DataType.F32_Default, DataType.F64_Default) \
    .dtype_format(DataType.F16_Default, DataType.I64_Default, DataType.F32_Default, DataType.F16_Default) \
    .dtype_format(DataType.F32_Default, DataType.I64_Default, DataType.F32_Default, DataType.F32_Default) \
    .dtype_format(DataType.F64_Default, DataType.I64_Default, DataType.F32_Default, DataType.F64_Default) \
    .get_op_info()


@op_info_register(upsample_trilinear_3d_grad_op_info)
def _upsample_trilinear_3d_grad_aicpu():
    """UpsampleTrilinear3DGrad AiCPU register"""
    return
