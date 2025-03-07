#   Copyright (c) 2018 PaddlePaddle Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import unittest

import gradient_checker
import numpy as np
from decorator_helper import prog_scope
from eager_op_test import (
    OpTest,
    convert_float_to_uint16,
    convert_uint16_to_float,
)

import paddle
from paddle import fluid
from paddle.fluid import Program, core, program_guard


def cast_wrapper(x, out_dtype=None):
    return paddle.cast(x, paddle.dtype(out_dtype))


class TestCastOpFp32ToFp64(OpTest):
    def setUp(self):
        ipt = np.random.random(size=[10, 10])
        self.inputs = {'X': ipt.astype('float32')}
        self.outputs = {'Out': ipt.astype('float64')}
        self.attrs = {
            'in_dtype': int(core.VarDesc.VarType.FP32),
            'out_dtype': int(core.VarDesc.VarType.FP64),
        }
        self.op_type = 'cast'
        self.prim_op_type = "prim"
        self.python_api = cast_wrapper
        self.public_python_api = cast_wrapper

    def test_check_output(self):
        self.check_output()

    def test_grad(self):
        self.check_grad(['X'], ['Out'], check_prim=True)


class TestCastOpFp16ToFp32(OpTest):
    def setUp(self):
        ipt = np.random.random(size=[10, 10])
        self.inputs = {'X': ipt.astype('float16')}
        self.outputs = {'Out': ipt.astype('float32')}
        self.attrs = {
            'in_dtype': int(core.VarDesc.VarType.FP16),
            'out_dtype': int(core.VarDesc.VarType.FP32),
        }
        self.op_type = 'cast'
        self.prim_op_type = "prim"
        self.python_api = cast_wrapper
        self.public_python_api = cast_wrapper

    def test_check_output(self):
        self.check_output()

    def test_grad(self):
        self.check_grad(['X'], ['Out'], check_prim=True, only_check_prim=True)


class TestCastOpFp32ToFp16(OpTest):
    def setUp(self):
        ipt = np.random.random(size=[10, 10])
        self.inputs = {'X': ipt.astype('float32')}
        self.outputs = {'Out': ipt.astype('float16')}
        self.attrs = {
            'in_dtype': int(core.VarDesc.VarType.FP32),
            'out_dtype': int(core.VarDesc.VarType.FP16),
        }
        self.op_type = 'cast'
        self.prim_op_type = "prim"
        self.python_api = cast_wrapper
        self.public_python_api = cast_wrapper

    def test_check_output(self):
        self.check_output()

    def test_grad(self):
        self.check_grad(['X'], ['Out'], check_prim=True, only_check_prim=True)


class TestCastOpBf16ToFp32(OpTest):
    def setUp(self):
        ipt = np.array(np.random.randint(10, size=[10, 10])).astype('uint16')
        self.inputs = {'X': ipt}
        self.outputs = {'Out': convert_uint16_to_float(ipt)}
        self.attrs = {
            'in_dtype': int(core.VarDesc.VarType.BF16),
            'out_dtype': int(core.VarDesc.VarType.FP32),
        }
        self.op_type = 'cast'
        self.prim_op_type = "prim"
        self.python_api = cast_wrapper
        self.public_python_api = cast_wrapper
        self.if_enable_cinn()

    def if_enable_cinn(self):
        self.enable_cinn = False

    def test_check_output(self):
        self.check_output()

    def test_grad(self):
        self.check_grad(['X'], ['Out'], check_prim=True, only_check_prim=True)


class TestCastOpFp32ToBf16(OpTest):
    def setUp(self):
        ipt = np.random.random(size=[10, 10]).astype('float32')
        self.inputs = {'X': ipt}
        self.outputs = {'Out': convert_float_to_uint16(ipt)}
        self.attrs = {
            'in_dtype': int(core.VarDesc.VarType.FP32),
            'out_dtype': int(core.VarDesc.VarType.BF16),
        }
        self.op_type = 'cast'
        self.prim_op_type = "prim"
        self.python_api = cast_wrapper
        self.public_python_api = cast_wrapper
        self.if_enable_cinn()

    def if_enable_cinn(self):
        self.enable_cinn = False

    def test_check_output(self):
        self.check_output()

    def test_grad(self):
        self.check_grad(['X'], ['Out'], check_prim=True, only_check_prim=True)


class TestCastOpError(unittest.TestCase):
    def test_errors(self):
        with program_guard(Program(), Program()):
            # The input type of cast_op must be Variable.
            x1 = fluid.create_lod_tensor(
                np.array([[-1]]), [[1]], fluid.CPUPlace()
            )
            self.assertRaises(TypeError, paddle.cast, x1, 'int32')


class TestCastOpEager(unittest.TestCase):
    def test_eager(self):
        with paddle.fluid.dygraph.base.guard():
            x = paddle.ones([2, 2], dtype="float16")
            x.stop_gradient = False
            out = paddle.cast(x, "float32")
            np.testing.assert_array_equal(
                out, np.ones([2, 2]).astype('float32')
            )
            out.backward()
            np.testing.assert_array_equal(x.gradient(), x.numpy())
            self.assertTrue(x.gradient().dtype == np.float16)


class TestCastDoubleGradCheck(unittest.TestCase):
    def cast_wrapper(self, x):
        return paddle.cast(x[0], 'float64')

    @prog_scope()
    def func(self, place):
        # the shape of input variable should be clearly specified, not inlcude -1.
        eps = 0.005
        dtype = np.float32

        data = paddle.static.data('data', [2, 3, 4], dtype)
        data.persistable = True
        out = paddle.cast(data, 'float64')
        data_arr = np.random.uniform(-1, 1, data.shape).astype(dtype)

        gradient_checker.double_grad_check(
            [data], out, x_init=[data_arr], place=place, eps=eps
        )
        gradient_checker.double_grad_check_for_dygraph(
            self.cast_wrapper, [data], out, x_init=[data_arr], place=place
        )

    def test_grad(self):
        paddle.enable_static()
        places = [fluid.CPUPlace()]
        if core.is_compiled_with_cuda():
            places.append(fluid.CUDAPlace(0))
        for p in places:
            self.func(p)


class TestCastTripleGradCheck(unittest.TestCase):
    def cast_wrapper(self, x):
        return paddle.cast(x[0], 'float64')

    @prog_scope()
    def func(self, place):
        # the shape of input variable should be clearly specified, not inlcude -1.
        eps = 0.005
        dtype = np.float32

        data = paddle.static.data('data', [2, 3, 4], dtype)
        data.persistable = True
        out = paddle.cast(data, 'float64')
        data_arr = np.random.uniform(-1, 1, data.shape).astype(dtype)

        gradient_checker.triple_grad_check(
            [data], out, x_init=[data_arr], place=place, eps=eps
        )
        gradient_checker.triple_grad_check_for_dygraph(
            self.cast_wrapper, [data], out, x_init=[data_arr], place=place
        )

    def test_grad(self):
        paddle.enable_static()
        places = [fluid.CPUPlace()]
        if core.is_compiled_with_cuda():
            places.append(fluid.CUDAPlace(0))
        for p in places:
            self.func(p)


if __name__ == '__main__':
    paddle.enable_static()
    unittest.main()
