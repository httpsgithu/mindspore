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
from tests.mark_utils import arg_mark

import numpy as np
import pytest

import mindspore.context as context
import mindspore.nn as nn
from mindspore import Tensor, ops
from mindspore.ops import operations as P


class OpNetWrapper(nn.Cell):
    def __init__(self, op):
        super(OpNetWrapper, self).__init__()
        self.op = op

    def construct(self, *inputs):
        return self.op(*inputs)


class GreaterEqualFunc(nn.Cell):
    def construct(self, *inputs):
        return ops.ge(*inputs)


@arg_mark(plat_marks=['cpu_linux', 'cpu_windows', 'cpu_macos'], level_mark='level0', card_mark='onecard', essential_mark='essential')
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
@pytest.mark.parametrize('dtype', [np.int8, np.int16, np.int32, np.int64, np.float16, np.float32, np.float64])
def test_greater_equal_op_dtype_1(mode, dtype):
    """
    Feature: Test GreaterEqual op.
    Description: Test GreaterEqual with dtype input.
    Expectation: The result match to the expect value.
    """
    context.set_context(mode=mode, device_target="CPU")

    op = P.GreaterEqual()
    op_wrapper = OpNetWrapper(op)

    input_x = Tensor(np.array([1, -2, 3]).astype(dtype))
    input_y = Tensor(np.array([3, -2, 1]).astype(dtype))
    outputs = op_wrapper(input_x, input_y)

    assert outputs.shape == (3,)
    assert np.allclose(outputs.asnumpy(), [False, True, True])


@arg_mark(plat_marks=['cpu_linux', 'cpu_windows', 'cpu_macos'], level_mark='level0', card_mark='onecard', essential_mark='essential')
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
@pytest.mark.parametrize('dtype', [np.uint8, np.uint16, np.uint32, np.uint64])
def test_greater_equal_op_dtype_2(mode, dtype):
    """
    Feature: Test GreaterEqual op.
    Description: Test GreaterEqual with dtype input.
    Expectation: The result match to the expect value.
    """
    context.set_context(mode=mode, device_target="CPU")

    op = P.GreaterEqual()
    op_wrapper = OpNetWrapper(op)

    input_x = Tensor(np.array([1, 2, 3]).astype(dtype))
    input_y = Tensor(np.array([3, 2, 1]).astype(dtype))
    outputs = op_wrapper(input_x, input_y)

    assert outputs.shape == (3,)
    assert np.allclose(outputs.asnumpy(), [False, True, True])


@arg_mark(plat_marks=['cpu_linux', 'cpu_windows', 'cpu_macos'], level_mark='level0', card_mark='onecard', essential_mark='essential')
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
@pytest.mark.parametrize('dtype', [np.bool])
def test_greater_equal_op_dtype_3(mode, dtype):
    """
    Feature: Test GreaterEqual op.
    Description: Test GreaterEqual with dtype input.
    Expectation: The result match to the expect value.
    """
    context.set_context(mode=mode, device_target="CPU")

    op = P.GreaterEqual()
    op_wrapper = OpNetWrapper(op)

    input_x = Tensor(np.array([False, True, True]).astype(dtype))
    input_y = Tensor(np.array([True, True, False]).astype(dtype))
    outputs = op_wrapper(input_x, input_y)

    assert outputs.shape == (3,)
    assert np.allclose(outputs.asnumpy(), [False, True, True])


@arg_mark(plat_marks=['cpu_linux', 'cpu_windows', 'cpu_macos'], level_mark='level0', card_mark='onecard', essential_mark='essential')
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_greater_equal_op_functional(mode):
    """
    Feature: Test GreaterEqual op.
    Description: Test GreaterEqual with functional.
    Expectation: The result match to the expect value.
    """
    context.set_context(mode=mode, device_target="CPU")

    op_wrapper = GreaterEqualFunc()

    input_x = Tensor(np.array([1, 2, 3]).astype(np.float32))
    input_y = Tensor(np.array([3, 2, 1]).astype(np.float32))
    outputs = op_wrapper(input_x, input_y)

    assert outputs.shape == (3,)
    assert np.allclose(outputs.asnumpy(), [False, True, True])


@arg_mark(plat_marks=['cpu_linux', 'cpu_windows', 'cpu_macos'], level_mark='level0', card_mark='onecard', essential_mark='essential')
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_greater_equal_op_tensor(mode):
    """
    Feature: Test GreaterEqual op.
    Description: Test GreaterEqual with Tensor.
    Expectation: The result match to the expect value.
    """
    context.set_context(mode=mode, device_target="CPU")

    input_x = Tensor(np.array([1, 2, 3]).astype(np.float32))
    input_y = Tensor(np.array([3, 2, 1]).astype(np.float32))
    outputs = input_x.ge(input_y)

    assert outputs.shape == (3,)
    assert np.allclose(outputs.asnumpy(), [False, True, True])
