# Copyright 2024 Huawei Technologies Co., Ltd
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
import numpy as np

import mindspore as ms
from mindspore import nn, ops
import mindspore.communication as D

class Net(nn.Cell):
    def __init__(self):
        super(Net, self).__init__()
        self.ops = ops.Sin()
        self.ops.shard(((2, 2, 2),))
    def construct(self, x):
        return self.ops(x)


def test_graph_mode():
    '''
    Feature: Parallel Support for Complex64 input
    Description: graph mode
    Expectation: Run success
    '''
    ms.set_context(mode=ms.GRAPH_MODE)
    ms.set_auto_parallel_context(parallel_mode=ms.ParallelMode.SEMI_AUTO_PARALLEL, dataset_strategy="full_batch")
    # Parallel in the case of complex input only supports KernelByKernel mode by now. So we set 'jit_level' to 'O0'.
    ms.set_context(jit_level='O0')

    D.init()
    ms.set_seed(1)

    x_real = np.random.randn(4, 4, 4).astype(np.float32)
    x_imag = np.random.randn(4, 4, 4).astype(np.float32)
    x = ms.Tensor(x_real + 1j*x_imag)

    sin_x = np.sin(x_real + 1j*x_imag)

    net = Net()
    output = net(x)
    output_np = output.asnumpy()

    assert np.allclose(np.real(output_np), np.real(sin_x)) and np.allclose(np.imag(output_np), np.imag(sin_x))
    ms.reset_auto_parallel_context()
