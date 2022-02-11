# Copyright 2021 Huawei Technologies Co., Ltd
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
"""st for scipy.linalg."""

from typing import Generic
from functools import reduce
import pytest
import numpy as onp
import scipy as osp

import mindspore.nn as nn
import mindspore.scipy as msp
from mindspore import context, Tensor
import mindspore.numpy as mnp
from mindspore.scipy.linalg import det, solve_triangular
from tests.st.scipy_st.utils import match_array, create_full_rank_matrix, create_sym_pos_matrix, \
    create_random_rank_matrix, match_exception_info

onp.random.seed(0)
context.set_context(mode=context.PYNATIVE_MODE)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('args', [(), (1,), (7, -1), (3, 4, 5),
                                  (onp.ones((3, 4), dtype=onp.float32), 5, onp.random.randn(5, 2).astype(onp.float32))])
def test_block_diag(args):
    """
    Feature: ALL TO ALL
    Description: test cases for block_diag
    Expectation: the result match scipy
    """
    tensor_args = tuple([Tensor(arg) for arg in args])
    ms_res = msp.linalg.block_diag(*tensor_args)

    scipy_res = osp.linalg.block_diag(*args)
    match_array(ms_res.asnumpy(), scipy_res)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('n', [10, 20, 52])
@pytest.mark.parametrize('trans', ["N", "T", "C"])
@pytest.mark.parametrize('dtype', [onp.float32, onp.float64, onp.int32, onp.int64])
@pytest.mark.parametrize('lower', [False, True])
@pytest.mark.parametrize('unit_diagonal', [False, True])
def test_solve_triangular(n: int, dtype, lower: bool, unit_diagonal: bool, trans: str):
    """
    Feature: ALL TO ALL
    Description:  test cases for solve_triangular for batched triangular matrix solver [..., N, N]
    Expectation: the result match scipy solve_triangular result
    """
    rtol, atol = 1.e-5, 1.e-8
    if dtype == onp.float32:
        rtol, atol = 1.e-3, 1.e-3

    onp.random.seed(0)
    a = create_random_rank_matrix((n, n), dtype)
    b = create_random_rank_matrix((n,), dtype)

    output = solve_triangular(Tensor(a), Tensor(b), trans, lower, unit_diagonal).asnumpy()
    expect = osp.linalg.solve_triangular(a, b, lower=lower, unit_diagonal=unit_diagonal,
                                         trans=trans)

    assert onp.allclose(expect, output, rtol=rtol, atol=atol)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('n', [10, 20, 15])
@pytest.mark.parametrize('batch', [(3,), (4, 5)])
@pytest.mark.parametrize('trans', ["N", "T", "C"])
@pytest.mark.parametrize('dtype', [onp.float32, onp.float64, onp.int32, onp.int64])
@pytest.mark.parametrize('lower', [False, True])
@pytest.mark.parametrize('unit_diagonal', [False, True])
def test_solve_triangular_batched(n: int, batch, dtype, lower: bool, unit_diagonal: bool, trans: str):
    """
    Feature: ALL TO ALL
    Description:  test cases for solve_triangular for batched triangular matrix solver [..., N, N]
    Expectation: the result match scipy solve_triangular result
    """
    rtol, atol = 1.e-5, 1.e-8
    if dtype == onp.float32:
        rtol, atol = 1.e-3, 1.e-3

    onp.random.seed(0)
    a = create_random_rank_matrix(batch + (n, n), dtype)
    b = create_random_rank_matrix(batch + (n,), dtype)

    # mindspore
    output = solve_triangular(Tensor(a), Tensor(b), trans, lower, unit_diagonal).asnumpy()

    # scipy
    batch_num = reduce(lambda x, y: x * y, batch)
    a_array = a.reshape((batch_num, n, n))
    b_array = b.reshape((batch_num, n))
    expect = onp.stack([osp.linalg.solve_triangular(a_array[i, :], b_array[i, :], lower=lower,
                                                    unit_diagonal=unit_diagonal, trans=trans)
                        for i in range(batch_num)])
    expect = expect.reshape(output.shape)

    assert onp.allclose(expect, output, rtol=rtol, atol=atol)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_solve_triangular_error_dims():
    """
    Feature: ALL TO ALL
    Description:  test cases for solve_triangular for batched triangular matrix solver [..., N, N]
    Expectation: solve_triangular raises expectated Exception
    """
    # matrix a is 1D
    a = create_random_rank_matrix((10,), dtype=onp.float32)
    b = create_random_rank_matrix((10,), dtype=onp.float32)
    with pytest.raises(ValueError) as err:
        solve_triangular(Tensor(a), Tensor(b))
    msg = "For 'SolveTriangular', the dimension of 'a' should be at least 2, but got 1 dimensions."
    assert match_exception_info(err, msg)

    # matrix a is not square matrix
    a = create_random_rank_matrix((4, 5), dtype=onp.float32)
    b = create_random_rank_matrix((10,), dtype=onp.float32)
    with pytest.raises(ValueError) as err:
        solve_triangular(Tensor(a), Tensor(b))
    msg = "For 'SolveTriangular', the last two dimensions of 'a' should be the same, " \
          "but got shape of [4, 5]. Please make sure that the shape of 'a' be like [..., N, N]"
    assert match_exception_info(err, msg)

    a = create_random_rank_matrix((3, 5, 4, 5), dtype=onp.float32)
    b = create_random_rank_matrix((3, 5, 10,), dtype=onp.float32)
    with pytest.raises(ValueError) as err:
        solve_triangular(Tensor(a), Tensor(b))
    msg = "For 'SolveTriangular', the last two dimensions of 'a' should be the same," \
          " but got shape of [3, 5, 4, 5]. Please make sure that the shape of 'a' be like [..., N, N]"
    assert match_exception_info(err, msg)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_solve_triangular_error_dims_mismatched():
    """
    Feature: ALL TO ALL
    Description:  test cases for solve_triangular for batched triangular matrix solver [..., N, N]
    Expectation: solve_triangular raises expectated Exception
    """
    # dimension of a and b is not matched
    a = create_random_rank_matrix((3, 4, 5, 5), dtype=onp.float32)
    b = create_random_rank_matrix((5, 10,), dtype=onp.float32)
    with pytest.raises(ValueError) as err:
        solve_triangular(Tensor(a), Tensor(b))
    msg = "For 'SolveTriangular', the dimension of 'b' should be 'a.dim' or 'a.dim' - 1, " \
          "which is 4 or 3, but got 2 dimensions."
    assert match_exception_info(err, msg)

    # last two dimensions not matched
    a = create_random_rank_matrix((3, 4, 5, 5), dtype=onp.float32)
    b = create_random_rank_matrix((5, 10, 4), dtype=onp.float32)
    with pytest.raises(ValueError) as err:
        solve_triangular(Tensor(a), Tensor(b))
    msg = "For 'SolveTriangular', the last two dimensions of 'a' and 'b' should be matched, " \
          "but got shape of [3, 4, 5, 5] and [5, 10, 4]. Please make sure that the shape of 'a' " \
          "and 'b' be like [..., N, N] X [..., N, M] or [..., N, N] X [..., N]."
    assert match_exception_info(err, msg)

    a = create_random_rank_matrix((3, 4, 5, 5), dtype=onp.float32)
    b = create_random_rank_matrix((5, 10, 4, 1), dtype=onp.float32)
    with pytest.raises(ValueError) as err:
        solve_triangular(Tensor(a), Tensor(b))
    msg = "For 'SolveTriangular', the last two dimensions of 'a' and 'b' should be matched, " \
          "but got shape of [3, 4, 5, 5] and [5, 10, 4, 1]. Please make sure that the shape of 'a' " \
          "and 'b' be like [..., N, N] X [..., N, M] or [..., N, N] X [..., N]."
    print(err.value)
    assert match_exception_info(err, msg)

    # batch dimensions not matched
    a = create_random_rank_matrix((3, 4, 5, 5), dtype=onp.float32)
    b = create_random_rank_matrix((5, 10, 5), dtype=onp.float32)
    with pytest.raises(ValueError) as err:
        solve_triangular(Tensor(a), Tensor(b))
    msg = "For 'SolveTriangular', the batch dimensions of 'a' and 'b' should all be the same, " \
          "but got shape of [3, 4, 5, 5] and [5, 10, 5]. Please make sure that " \
          "the shape of 'a' and 'b' be like [a, b, c, ..., N, N] X [a, b, c, ..., N, M] " \
          "or [a, b, c, ..., N, N] X [a, b, c, ..., N]."
    assert match_exception_info(err, msg)

    a = create_random_rank_matrix((3, 4, 5, 5), dtype=onp.float32)
    b = create_random_rank_matrix((5, 10, 5, 1), dtype=onp.float32)
    with pytest.raises(ValueError) as err:
        solve_triangular(Tensor(a), Tensor(b))
    msg = "For 'SolveTriangular', the batch dimensions of 'a' and 'b' should all be the same, " \
          "but got shape of [3, 4, 5, 5] and [5, 10, 5, 1]. Please make sure that " \
          "the shape of 'a' and 'b' be like [a, b, c, ..., N, N] X [a, b, c, ..., N, M] " \
          "or [a, b, c, ..., N, N] X [a, b, c, ..., N]."
    assert match_exception_info(err, msg)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_solve_triangular_error_tensor_dtype():
    """
    Feature: ALL TO ALL
    Description:  test cases for solve_triangular for batched triangular matrix solver [..., N, N]
    Expectation: solve_triangular raises expectated Exception
    """
    a = create_random_rank_matrix((10, 10), onp.float16)
    b = create_random_rank_matrix((10,), onp.float16)
    with pytest.raises(TypeError) as err:
        solve_triangular(Tensor(a), Tensor(b))
    msg = f"For 'SolveTriangular', the type of `a_dtype` should be in " \
          f"[mindspore.float32, mindspore.float64], but got Float16."
    assert match_exception_info(err, msg)

    a = create_random_rank_matrix((10, 10), onp.float32)
    b = create_random_rank_matrix((10,), onp.float16)
    with pytest.raises(TypeError) as err:
        solve_triangular(Tensor(a), Tensor(b))
    msg = f"For 'SolveTriangular', the type of `b_dtype` should be in " \
          f"[mindspore.float32, mindspore.float64], but got Float16."
    assert match_exception_info(err, msg)

    a = create_random_rank_matrix((10, 10), onp.float32)
    b = create_random_rank_matrix((10,), onp.float64)
    with pytest.raises(TypeError) as err:
        solve_triangular(Tensor(a), Tensor(b))
    msg = "For 'SolveTriangular' type of `b_dtype` should be same as `a_dtype`, " \
          "but `a_dtype` is Float32 and `b_dtype` is Float64."
    assert match_exception_info(err, msg)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('dtype', [onp.float32, onp.float64, onp.int32, onp.int64])
@pytest.mark.parametrize('argname, argtype', [('lower', 'bool'), ('overwrite_b', 'bool'), ('check_finite', 'bool')])
@pytest.mark.parametrize('wrong_argvalue, wrong_argtype', [(5.0, 'float'), (None, 'NoneType'), ('test', 'str')])
def test_solve_triangular_error_type(dtype, argname, argtype, wrong_argvalue, wrong_argtype):
    """
    Feature: ALL TO ALL
    Description:  test cases for solve_triangular for batched triangular matrix solver [..., N, N]
    Expectation: solve_triangular raises expectated Exception
    """
    a = onp.random.randint(low=-1024, high=1024, size=(10, 10)).astype(dtype)
    b = onp.random.randint(low=-1024, high=1024, size=(10,)).astype(dtype)

    kwargs = {argname: wrong_argvalue}
    with pytest.raises(TypeError) as err:
        solve_triangular(Tensor(a), Tensor(b), **kwargs)
    msg = f"For 'solve_triangular', the type of `{argname}` should be {argtype}, " \
          f"but got '{wrong_argvalue}' with type {wrong_argtype}."
    assert match_exception_info(err, msg)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('dtype', [onp.float32, onp.float64, onp.int32, onp.int64])
@pytest.mark.parametrize('wrong_argvalue, wrong_argtype', [(5.0, 'float'), (None, 'NoneType')])
def test_solve_triangular_error_type_trans(dtype, wrong_argvalue, wrong_argtype):
    """
    Feature: ALL TO ALL
    Description:  test cases for solve_triangular for batched triangular matrix solver [..., N, N]
    Expectation: solve_triangular raises expectated Exception
    """
    a = onp.random.randint(low=-1024, high=1024, size=(10, 10)).astype(dtype)
    b = onp.random.randint(low=-1024, high=1024, size=(10,)).astype(dtype)

    with pytest.raises(TypeError) as err:
        solve_triangular(Tensor(a), Tensor(b), trans=wrong_argvalue)
    msg = f"For 'solve_triangular', the type of `trans` should be one of ['int', 'str'], " \
          f"but got '{wrong_argvalue}' with type {wrong_argtype}."
    assert match_exception_info(err, msg)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_solve_triangular_error_tensor_type():
    """
    Feature: ALL TO ALL
    Description:  test cases for solve_triangular for batched triangular matrix solver [..., N, N]
    Expectation: solve_triangular raises expectated Exception
    """
    a = 'test'
    b = onp.random.randint(low=-1024, high=1024, size=(10,)).astype(onp.float32)
    with pytest.raises(TypeError) as err:
        solve_triangular(a, Tensor(b))
    msg = "For Primitive[DType], the input argument[infer type]must be a Tensor or CSRTensor but got String."
    assert match_exception_info(err, msg)

    a = [1, 2, 3]
    b = onp.random.randint(low=-1024, high=1024, size=(10,)).astype(onp.float32)
    with pytest.raises(TypeError) as err:
        solve_triangular(a, Tensor(b))
    msg = "For Primitive[DType], the input argument[infer type]must be a Tensor or CSRTensor but got List[Int64*3]."
    assert match_exception_info(err, msg)

    a = (1, 2, 3)
    b = onp.random.randint(low=-1024, high=1024, size=(10,)).astype(onp.float32)
    with pytest.raises(TypeError) as err:
        solve_triangular(a, Tensor(b))
    msg = "For Primitive[DType], the input argument[infer type]must be a Tensor or CSRTensor but got Tuple[Int64*3]."
    assert match_exception_info(err, msg)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('data_type', [onp.float32, onp.float64])
@pytest.mark.parametrize('shape', [(4, 4), (50, 50), (2, 5, 5)])
def test_inv(data_type, shape):
    """
    Feature: ALL TO ALL
    Description: test cases for inv
    Expectation: the result match numpy
    """
    onp.random.seed(0)
    x = create_full_rank_matrix(shape, data_type)

    ms_res = msp.linalg.inv(Tensor(x))
    scipy_res = onp.linalg.inv(x)
    match_array(ms_res.asnumpy(), scipy_res, error=3)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('n', [4, 5, 6])
@pytest.mark.parametrize('lower', [True, False])
@pytest.mark.parametrize('data_type', [onp.float32, onp.float64])
def test_cholesky(n: int, lower: bool, data_type: Generic):
    """
    Feature: ALL TO ALL
    Description:  test cases for cholesky [N,N]
    Expectation: the result match scipy cholesky
    """
    a = create_sym_pos_matrix((n, n), data_type)
    tensor_a = Tensor(a)
    rtol = 1.e-3
    atol = 1.e-3
    if data_type == onp.float64:
        rtol = 1.e-5
        atol = 1.e-8
    osp_c = osp.linalg.cholesky(a, lower=lower)
    msp_c = msp.linalg.cholesky(tensor_a, lower=lower)
    assert onp.allclose(osp_c, msp_c.asnumpy(), rtol=rtol, atol=atol)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('shape', [(3, 4, 4), (3, 5, 5), (2, 3, 5, 5)])
@pytest.mark.parametrize('lower', [True, False])
@pytest.mark.parametrize('data_type', [onp.float32, onp.float64])
def test_batch_cholesky(shape, lower: bool, data_type):
    """
    Feature: ALL To ALL
    Description: test cases for cholesky decomposition test cases for A[N,N]x = b[N,1]
    Expectation: the result match to scipy
    """
    b_s_l = list()
    b_s_a = list()
    tmp = onp.zeros(shape[:-2])
    inner_row = shape[-2]
    inner_col = shape[-1]
    for _, _ in onp.ndenumerate(tmp):
        a = create_sym_pos_matrix((inner_row, inner_col), data_type)
        s_l = osp.linalg.cholesky(a, lower)
        b_s_l.append(s_l)
        b_s_a.append(a)
    tensor_b_a = Tensor(onp.array(b_s_a))
    b_m_l = msp.linalg.cholesky(tensor_b_a, lower)
    b_s_l = onp.asarray(b_s_l).reshape(b_m_l.shape)
    rtol = 1.e-3
    atol = 1.e-3
    if data_type == onp.float64:
        rtol = 1.e-5
        atol = 1.e-8
    assert onp.allclose(b_m_l.asnumpy(), b_s_l, rtol=rtol, atol=atol)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('n', [4, 5, 6])
@pytest.mark.parametrize('lower', [True, False])
@pytest.mark.parametrize('data_type', [onp.float32, onp.float64])
def test_cho_factor(n: int, lower: bool, data_type: Generic):
    """
    Feature: ALL TO ALL
    Description:  test cases for cho_factor [N,N]
    Expectation: the result match scipy cholesky
    """
    a = create_sym_pos_matrix((n, n), data_type)
    tensor_a = Tensor(a)
    msp_c, _ = msp.linalg.cho_factor(tensor_a, lower=lower)
    osp_c, _ = osp.linalg.cho_factor(a, lower=lower)
    rtol = 1.e-3
    atol = 1.e-3
    if data_type == onp.float64:
        rtol = 1.e-5
        atol = 1.e-8
    assert onp.allclose(osp_c, msp_c.asnumpy(), rtol=rtol, atol=atol)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('n', [4, 5, 6])
@pytest.mark.parametrize('lower', [True, False])
@pytest.mark.parametrize('data_type', [onp.float64])
def test_cholesky_solve(n: int, lower: bool, data_type):
    """
    Feature: ALL TO ALL
    Description:  test cases for cholesky  solver [N,N]
    Expectation: the result match scipy cholesky_solve
    """
    a = create_sym_pos_matrix((n, n), data_type)
    b = onp.ones((n, 1), dtype=data_type)
    tensor_a = Tensor(a)
    tensor_b = Tensor(b)
    osp_c, lower = osp.linalg.cho_factor(a, lower=lower)
    msp_c, msp_lower = msp.linalg.cho_factor(tensor_a, lower=lower)
    osp_factor = (osp_c, lower)

    ms_cho_factor = (msp_c, msp_lower)
    osp_x = osp.linalg.cho_solve(osp_factor, b)
    msp_x = msp.linalg.cho_solve(ms_cho_factor, tensor_b)
    # pre tensor_a has been inplace.
    tensor_a = Tensor(a)
    assert onp.allclose(onp.dot(a, osp_x), mnp.dot(tensor_a, msp_x).asnumpy())


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('n', [4, 6, 9, 20])
@pytest.mark.parametrize('lower', [True, False])
@pytest.mark.parametrize('data_type, rtol, atol',
                         [(onp.int32, 1e-5, 1e-8), (onp.int64, 1e-5, 1e-8), (onp.float32, 1e-3, 1e-4),
                          (onp.float64, 1e-5, 1e-8)])
def test_eigh(n: int, lower, data_type, rtol, atol):
    """
    Feature: ALL TO ALL
    Description:  test cases for eigenvalues/eigenvector for symmetric/Hermitian matrix solver [N,N]
    Expectation: the result match scipy eigenvalues
    """
    onp.random.seed(0)
    a = create_sym_pos_matrix([n, n], data_type)
    a_tensor = Tensor(onp.array(a))

    # test for real scalar float
    w, v = msp.linalg.eigh(a_tensor, lower=lower, eigvals_only=False)
    lhs = a @ v.asnumpy()
    rhs = v.asnumpy() @ onp.diag(w.asnumpy())
    assert onp.allclose(lhs, rhs, rtol, atol)
    # test for real scalar float no vector
    w0 = msp.linalg.eigh(a_tensor, lower=lower, eigvals_only=True)
    assert onp.allclose(w.asnumpy(), w0.asnumpy(), rtol, atol)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('n', [4, 6, 9, 20])
@pytest.mark.parametrize('data_type', [(onp.complex64, "f"), (onp.complex128, "d")])
def test_eigh_complex(n: int, data_type):
    """
    Feature: ALL TO ALL
    Description:  test cases for eigenvalues/eigenvector for symmetric/Hermitian matrix solver [N,N]
    Expectation: the result match scipy eigenvalues
    """
    # test case for complex
    tol = {"f": (1e-3, 1e-4), "d": (1e-5, 1e-8)}
    rtol = tol[data_type[1]][0]
    atol = tol[data_type[1]][1]
    A = onp.array(onp.random.rand(n, n), dtype=data_type[0])
    for i in range(0, n):
        for j in range(0, n):
            if i == j:
                A[i][j] = complex(onp.random.rand(1, 1), 0)
            else:
                A[i][j] = complex(onp.random.rand(1, 1), onp.random.rand(1, 1))
    sym_al = (onp.tril((onp.tril(A) - onp.tril(A).T)) + onp.tril(A).conj().T)
    sym_au = (onp.triu((onp.triu(A) - onp.triu(A).T)) + onp.triu(A).conj().T)
    msp_wl, msp_vl = msp.linalg.eigh(Tensor(onp.array(sym_al).astype(data_type[0])), lower=True, eigvals_only=False)
    msp_wu, msp_vu = msp.linalg.eigh(Tensor(onp.array(sym_au).astype(data_type[0])), lower=False, eigvals_only=False)
    assert onp.allclose(sym_al @ msp_vl.asnumpy() - msp_vl.asnumpy() @ onp.diag(msp_wl.asnumpy()),
                        onp.zeros((n, n)), rtol, atol)
    assert onp.allclose(sym_au @ msp_vu.asnumpy() - msp_vu.asnumpy() @ onp.diag(msp_wu.asnumpy()),
                        onp.zeros((n, n)), rtol, atol)

    # test for real scalar complex no vector
    msp_wl0 = msp.linalg.eigh(Tensor(onp.array(sym_al).astype(data_type[0])), lower=True, eigvals_only=True)
    msp_wu0 = msp.linalg.eigh(Tensor(onp.array(sym_au).astype(data_type[0])), lower=False, eigvals_only=True)
    assert onp.allclose(msp_wl.asnumpy() - msp_wl0.asnumpy(), onp.zeros((n, n)), rtol, atol)
    assert onp.allclose(msp_wu.asnumpy() - msp_wu0.asnumpy(), onp.zeros((n, n)), rtol, atol)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('dtype', [onp.float32, onp.float64, onp.int32, onp.int64])
@pytest.mark.parametrize('argname, argtype',
                         [('lower', 'bool'), ('eigvals_only', 'bool'), ('overwrite_a', 'bool'), ('overwrite_b', 'bool'),
                          ('turbo', 'bool'), ('check_finite', 'bool')])
@pytest.mark.parametrize('wrong_argvalue, wrong_argtype', [(5.0, 'float'), (None, 'NoneType')])
def test_eigh_error_type(dtype, argname, argtype, wrong_argvalue, wrong_argtype):
    """
    Feature: ALL TO ALL
    Description:  test cases for solve_triangular for batched triangular matrix solver [..., N, N]
    Expectation: eigh raises expectated Exception
    """
    a = onp.random.randint(low=-1024, high=1024, size=(10, 10)).astype(dtype)
    b = onp.random.randint(low=-1024, high=1024, size=(10,)).astype(dtype)

    kwargs = {argname: wrong_argvalue}
    with pytest.raises(TypeError) as err:
        msp.linalg.eigh(Tensor(a), Tensor(b), **kwargs)
    assert str(err.value) == f"For 'eigh', the type of `{argname}` should be {argtype}, " \
                             f"but got '{wrong_argvalue}' with type {wrong_argtype}."


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('dtype, dtype_name', [(onp.float16, 'Float16'), (onp.int8, 'Int8'), (onp.int16, 'Int16')])
def test_eigh_error_tensor_dtype(dtype, dtype_name):
    """
    Feature: ALL TO ALL
    Description:  test cases for solve_triangular for batched triangular matrix solver [..., N, N]
    Expectation: eigh raises expectated Exception
    """
    a = onp.random.randint(low=-1024, high=1024, size=(10, 10)).astype(dtype)
    with pytest.raises(TypeError) as err:
        msp.linalg.eigh(Tensor(a))
    msg = f"For 'Eigh', the type of `A_dtype` should be in " \
          f"[mindspore.float32, mindspore.float64, mindspore.complex64, mindspore.complex128], but got {dtype_name}."
    assert match_exception_info(err, msg)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('n', [1, 3, 4, 6])
@pytest.mark.parametrize('dtype', [onp.float32, onp.float64, onp.int32, onp.int64])
def test_eigh_error_dims(n: int, dtype):
    """
    Feature: ALL TO ALL
    Description:  test cases for solve_triangular for batched triangular matrix solver [..., N, N]
    Expectation: eigh raises expectated Exception
    """
    a = onp.random.randint(low=-1024, high=1024, size=(10,) * n).astype(dtype)
    with pytest.raises(RuntimeError) as err:
        msp.linalg.eigh(Tensor(a))
    msg = f"Wrong array shape. For 'Eigh', a should be 2D, but got [{n}] dimensions."
    assert match_exception_info(err, msg)

    a = onp.random.randint(low=-1024, high=1024, size=(n, n + 1)).astype(dtype)
    with pytest.raises(RuntimeError) as err:
        msp.linalg.eigh(Tensor(a))
    msg = f"Wrong array shape. For 'Eigh', a should be a squre matrix like [N X N], " \
          f"but got [{n} X {n + 1}]."
    assert match_exception_info(err, msg)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_eigh_error_not_implemented():
    """
    Feature: ALL TO ALL
    Description:  test cases for solve_triangular for batched triangular matrix solver [..., N, N]
    Expectation: eigh raises expectated Exception
    """
    a = onp.random.randint(low=-1024, high=1024, size=(10, 10)).astype(onp.float32)
    b = onp.random.randint(low=-1024, high=1024, size=(10, 10)).astype(onp.float32)
    with pytest.raises(ValueError) as err:
        msp.linalg.eigh(Tensor(a), Tensor(b))
    msg = "Currently only case b=None of eigh is Implemented. Which means that b must be identity matrix."
    assert match_exception_info(err, msg)

    with pytest.raises(ValueError) as err:
        msp.linalg.eigh(Tensor(a), 42)
    msg = "Currently only case b=None of eigh is Implemented. Which means that b must be identity matrix."
    assert match_exception_info(err, msg)

    with pytest.raises(ValueError) as err:
        msp.linalg.eigh(Tensor(a), eigvals=42)
    msg = "Currently only case eigvals=None of eighis Implemented."
    assert match_exception_info(err, msg)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('shape', [(4, 4), (4, 5), (5, 10), (20, 20)])
@pytest.mark.parametrize('data_type', [onp.float32, onp.float64])
def test_lu(shape: (int, int), data_type):
    """
    Feature: ALL To ALL
    Description: test cases for lu decomposition test cases for A[N,N]x = b[N,1]
    Expectation: the result match to scipy
    """
    a = create_random_rank_matrix(shape, data_type)
    s_p, s_l, s_u = osp.linalg.lu(a)
    tensor_a = Tensor(a)
    m_p, m_l, m_u = msp.linalg.lu(tensor_a)
    rtol = 1.e-5
    atol = 1.e-5
    assert onp.allclose(m_p.asnumpy(), s_p, rtol=rtol, atol=atol)
    assert onp.allclose(m_l.asnumpy(), s_l, rtol=rtol, atol=atol)
    assert onp.allclose(m_u.asnumpy(), s_u, rtol=rtol, atol=atol)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('shape', [(3, 4, 4), (3, 4, 5), (2, 3, 4, 5)])
@pytest.mark.parametrize('data_type', [onp.float32, onp.float64])
def test_batch_lu(shape, data_type):
    """
    Feature: ALL To ALL
    Description: test cases for lu decomposition test cases for A[N,N]x = b[N,1]
    Expectation: the result match to scipy
    """
    b_a = create_random_rank_matrix(shape, data_type)
    b_s_p = list()
    b_s_l = list()
    b_s_u = list()
    tmp = onp.zeros(b_a.shape[:-2])
    for index, _ in onp.ndenumerate(tmp):
        a = b_a[index]
        s_p, s_l, s_u = osp.linalg.lu(a)
        b_s_p.append(s_p)
        b_s_l.append(s_l)
        b_s_u.append(s_u)
    tensor_b_a = Tensor(onp.array(b_a))
    b_m_p, b_m_l, b_m_u = msp.linalg.lu(tensor_b_a)
    b_s_p = onp.asarray(b_s_p).reshape(b_m_p.shape)
    b_s_l = onp.asarray(b_s_l).reshape(b_m_l.shape)
    b_s_u = onp.asarray(b_s_u).reshape(b_m_u.shape)
    rtol = 1.e-5
    atol = 1.e-5
    assert onp.allclose(b_m_p.asnumpy(), b_s_p, rtol=rtol, atol=atol)
    assert onp.allclose(b_m_l.asnumpy(), b_s_l, rtol=rtol, atol=atol)
    assert onp.allclose(b_m_u.asnumpy(), b_s_u, rtol=rtol, atol=atol)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('n', [4, 5, 10, 20])
@pytest.mark.parametrize('data_type', [onp.float32, onp.float64])
def test_lu_factor(n: int, data_type):
    """
    Feature: ALL To ALL
    Description: test cases for lu decomposition test cases for A[N,N]x = b[N,1]
    Expectation: the result match to scipy
    """
    a = create_full_rank_matrix((n, n), data_type)
    s_lu, s_pivots = osp.linalg.lu_factor(a)
    tensor_a = Tensor(a)
    m_lu, m_pivots = msp.linalg.lu_factor(tensor_a)
    rtol = 1.e-5
    atol = 1.e-5
    assert onp.allclose(m_lu.asnumpy(), s_lu, rtol=rtol, atol=atol)
    assert onp.allclose(m_pivots.asnumpy(), s_pivots, rtol=rtol, atol=atol)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('n', [4, 5, 10, 20])
@pytest.mark.parametrize('data_type', [onp.float32, onp.float64])
def test_lu_solve(n: int, data_type):
    """
    Feature: ALL To ALL
    Description: test cases for lu_solve test cases for A[N,N]x = b[N,1]
    Expectation: the result match to scipy
    """
    a = create_full_rank_matrix((n, n), data_type)
    b = onp.random.random((n, 1)).astype(data_type)
    rtol = 1.e-3
    atol = 1.e-3
    if data_type == onp.float64:
        rtol = 1.e-5
        atol = 1.e-8

    s_lu, s_piv = osp.linalg.lu_factor(a)
    m_lu, m_piv = msp.linalg.lu_factor(Tensor(a))
    assert onp.allclose(m_lu.asnumpy(), s_lu, rtol=rtol, atol=atol)
    assert onp.allclose(m_piv.asnumpy(), s_piv, rtol=rtol, atol=atol)

    osp_lu_factor = (s_lu, s_piv)
    msp_lu_factor = (m_lu, m_piv)
    osp_x = osp.linalg.lu_solve(osp_lu_factor, b)
    msp_x = msp.linalg.lu_solve(msp_lu_factor, Tensor(b))
    assert onp.allclose(msp_x.asnumpy(), osp_x, rtol=rtol, atol=atol)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('shape', [(3, 3), (5, 5), (10, 10), (20, 20)])
@pytest.mark.parametrize('dtype', [onp.float32, onp.float64])
def test_det(shape, dtype):
    """
    Feature: ALL To ALL
    Description: test cases for det
    Expectation: the result match to scipy
    """
    a = onp.random.random(shape).astype(dtype)
    sp_det = osp.linalg.det(a)
    tensor_a = Tensor(a)
    ms_det = msp.linalg.det(tensor_a)
    rtol = 1.e-5
    atol = 1.e-5
    assert onp.allclose(ms_det.asnumpy(), sp_det, rtol=rtol, atol=atol)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('shape', [(2, 3, 3), (2, 3, 5, 5)])
@pytest.mark.parametrize('dtype', [onp.float32, onp.float64])
def test_batch_det(shape, dtype):
    """
    Feature: ALL To ALL
    Description: test batch cases for det
    Expectation: the result match to scipy
    """
    a = onp.random.random(shape).astype(dtype)
    tensor_a = Tensor(a)
    ms_det = msp.linalg.det(tensor_a)
    sp_det = onp.empty(shape=ms_det.shape, dtype=dtype)
    for index, _ in onp.ndenumerate(sp_det):
        sp_det[index] = osp.linalg.det(a[index])
    rtol = 1.e-5
    atol = 1.e-5
    assert onp.allclose(ms_det.asnumpy(), sp_det, rtol=rtol, atol=atol)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('args', [(), (1,), (7, -1), (3, 4, 5),
                                  (onp.ones((3, 4), dtype=onp.float32), 5, onp.random.randn(5, 2).astype(onp.float32))])
def test_block_diag_graph(args):
    """
    Feature: ALL TO ALL
    Description: test cases for block_diag in graph mode
    Expectation: the result match scipy
    """
    context.set_context(mode=context.GRAPH_MODE)

    class TestNet(nn.Cell):
        def construct(self, inputs):
            return msp.linalg.block_diag(*inputs)

    tensor_args = tuple([Tensor(arg) for arg in args])
    ms_res = TestNet()(tensor_args)

    scipy_res = osp.linalg.block_diag(*args)
    match_array(ms_res.asnumpy(), scipy_res)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('shape', [(3, 3), (5, 5), (10, 10), (20, 20)])
@pytest.mark.parametrize('dtype', [onp.float32, onp.float64])
def test_det_graph(shape, dtype):
    """
    Feature: ALL To ALL
    Description: test cases for det in graph mode
    Expectation: the result match to scipy
    """
    context.set_context(mode=context.GRAPH_MODE)

    class TestNet(nn.Cell):
        def construct(self, a):
            return det(a)

    a = onp.random.random(shape).astype(dtype)
    sp_det = osp.linalg.det(a)
    tensor_a = Tensor(a)
    ms_det = TestNet()(tensor_a)
    rtol = 1.e-5
    atol = 1.e-5
    assert onp.allclose(ms_det.asnumpy(), sp_det, rtol=rtol, atol=atol)
