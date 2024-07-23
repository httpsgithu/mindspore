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
import mindspore.nn as nn
import mindspore.context as context
from mindspore import Tensor
import mindspore.ops.operations.nn_ops as ops
import mindspore.ops.operations._grad_ops as grad_ops


class NetFractionalMaxPoolWithFixedKsize(nn.Cell):
    def __init__(self, ksize, output_shape):
        super(NetFractionalMaxPoolWithFixedKsize, self).__init__()
        self.fractional_max_pool_with_fixed_ksize = ops.FractionalMaxPoolWithFixedKsize(ksize, output_shape)

    def construct(self, x, random_sapmples):
        return self.fractional_max_pool_with_fixed_ksize(x, random_sapmples)


class NetFractionalMaxPoolGradWithFixedKsize(nn.Cell):
    def __init__(self):
        super(NetFractionalMaxPoolGradWithFixedKsize, self).__init__()
        self.fractional_max_pool_grad_with_fixed_ksize = grad_ops.FractionalMaxPoolGradWithFixedKsize()

    def construct(self, origin_input, out_backprop, argmax):
        return self.fractional_max_pool_grad_with_fixed_ksize(origin_input, out_backprop, argmax)


@arg_mark(plat_marks=['platform_gpu'], level_mark='level1', card_mark='onecard', essential_mark='unessential')
def test_fractionalmaxpoolwithfixedksize():
    """
    Feature: FractionalMaxPoolWithFixedKsize
    Description: Test of input
    Expectation: The results are as expected
    """
    context_mode_types = [context.GRAPH_MODE, context.PYNATIVE_MODE]
    types_input1 = [np.float16, np.float32, np.int32, np.int64]
    types_input2 = [np.float16, np.float32]
    for context_mode_type in context_mode_types:
        context.set_context(mode=context_mode_type, device_target='GPU')
        for type_input1 in types_input1:
            for type_input2 in types_input2:
                x_np = np.array([i+1 for i in range(16)]).reshape([1, 1, 4, 4]).astype(type_input1)
                x_ms = Tensor(x_np)
                random_samples = Tensor(np.array([0.5, 0.8]).reshape([1, 1, 2]).astype(type_input2))
                ksize = (1, 1)
                output_shape = (2, 3)
                net = NetFractionalMaxPoolWithFixedKsize(ksize, output_shape)
                output_ms, argmax = net(x_ms, random_samples)
                expect_output = np.array([[[[1, 3, 4],
                                            [13, 15, 16]]]]).astype(type_input1)
                expect_output_argmax = np.array([[[[0, 2, 3],
                                                   [12, 14, 15]]]]).astype(type_input2)
                assert np.allclose(output_ms.asnumpy(), expect_output)
                assert np.allclose(argmax.asnumpy(), expect_output_argmax)

                out_backprop = Tensor(np.array([i+1 for i in range(6)]).reshape([1, 1, 2, 3]).astype(type_input1))
                net_grad = NetFractionalMaxPoolGradWithFixedKsize()
                output_grad = net_grad(x_ms, out_backprop, argmax)
                expect_output_grad = np.array([[[[1, 0, 2, 3],
                                                 [0, 0, 0, 0],
                                                 [0, 0, 0, 0],
                                                 [4, 0, 5, 6]]]]).astype(type_input2)
                assert np.allclose(output_grad.asnumpy(), expect_output_grad)
