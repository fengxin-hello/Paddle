#   Copyright (c) 2020 PaddlePaddle Authors. All Rights Reserved.
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

import numpy as np
from eager_op_test import OpTest, convert_float_to_uint16

import paddle
from paddle.fluid import core


def np_masked_select(x, mask):
    result = np.empty(shape=(0), dtype=x.dtype)
    for ele, ma in zip(np.nditer(x), np.nditer(mask)):
        if ma:
            result = np.append(result, ele)
    return result.flatten()


class TestMaskedSelectOp(OpTest):
    def setUp(self):
        self.init()
        self.op_type = "masked_select"
        self.python_api = paddle.masked_select
        x = np.random.random(self.shape).astype("float64")
        mask = np.array(np.random.randint(2, size=self.shape, dtype=bool))
        out = np_masked_select(x, mask)
        self.inputs = {'X': x, 'Mask': mask}
        self.outputs = {'Y': out}

    def test_check_output(self):
        self.check_output()

    def test_check_grad(self):
        self.check_grad(['X'], 'Y')

    def init(self):
        self.shape = (50, 3)


class TestMaskedSelectOp1(TestMaskedSelectOp):
    def init(self):
        self.shape = (6, 8, 9, 18)


class TestMaskedSelectOp2(TestMaskedSelectOp):
    def init(self):
        self.shape = (168,)


class TestMaskedSelectFP16Op(OpTest):
    def setUp(self):
        self.init()
        self.op_type = "masked_select"
        self.dtype = np.float16
        self.python_api = paddle.masked_select
        x = np.random.random(self.shape).astype("float16")
        mask = np.array(np.random.randint(2, size=self.shape, dtype=bool))
        out = np_masked_select(x, mask)
        self.inputs = {'X': x, 'Mask': mask}
        self.outputs = {'Y': out}

    def test_check_output(self):
        self.check_output()

    def test_check_grad(self):
        self.check_grad(['X'], 'Y')

    def init(self):
        self.shape = (50, 3)


class TestMaskedSelectFP16Op1(TestMaskedSelectFP16Op):
    def init(self):
        self.shape = (6, 8, 9, 18)


class TestMaskedSelectFP16Op2(TestMaskedSelectFP16Op):
    def init(self):
        self.shape = (168,)


@unittest.skipIf(
    not core.is_compiled_with_cuda()
    or not core.is_bfloat16_supported(core.CUDAPlace(0)),
    "core is not compiled with CUDA or not support bfloat16",
)
class TestMaskedSelectBF16Op(OpTest):
    def setUp(self):
        self.init()
        self.op_type = "masked_select"
        self.dtype = np.uint16
        self.python_api = paddle.masked_select
        x = np.random.random(self.shape).astype("float32")
        mask = np.array(np.random.randint(2, size=self.shape, dtype=bool))
        out = np_masked_select(x, mask)
        self.inputs = {'X': convert_float_to_uint16(x), 'Mask': mask}
        self.outputs = {'Y': convert_float_to_uint16(out)}

    def test_check_output(self):
        self.check_output_with_place(core.CUDAPlace(0))

    def test_check_grad(self):
        self.check_grad_with_place(core.CUDAPlace(0), ['X'], 'Y')

    def init(self):
        self.shape = (50, 3)


class TestMaskedSelectBF16Op1(TestMaskedSelectBF16Op):
    def init(self):
        self.shape = (6, 8, 9, 2)


class TestMaskedSelectBF16Op2(TestMaskedSelectBF16Op):
    def init(self):
        self.shape = (168,)


class TestMaskedSelectAPI(unittest.TestCase):
    def test_imperative_mode(self):
        paddle.disable_static()
        shape = (88, 6, 8)
        np_x = np.random.random(shape).astype('float32')
        np_mask = np.array(np.random.randint(2, size=shape, dtype=bool))
        x = paddle.to_tensor(np_x)
        mask = paddle.to_tensor(np_mask)
        out = paddle.masked_select(x, mask)
        np_out = np_masked_select(np_x, np_mask)
        np.testing.assert_allclose(out.numpy(), np_out, rtol=1e-05)
        paddle.enable_static()

    def test_static_mode(self):
        shape = [8, 9, 6]
        x = paddle.static.data(shape=shape, dtype='float32', name='x')
        mask = paddle.static.data(shape=shape, dtype='bool', name='mask')
        np_x = np.random.random(shape).astype('float32')
        np_mask = np.array(np.random.randint(2, size=shape, dtype=bool))

        out = paddle.masked_select(x, mask)
        np_out = np_masked_select(np_x, np_mask)

        exe = paddle.static.Executor(place=paddle.CPUPlace())

        (res,) = exe.run(
            paddle.static.default_main_program(),
            feed={"x": np_x, "mask": np_mask},
            fetch_list=[out],
        )
        np.testing.assert_allclose(res, np_out, rtol=1e-05)


class TestMaskedSelectError(unittest.TestCase):
    def test_error(self):
        with paddle.static.program_guard(
            paddle.static.Program(), paddle.static.Program()
        ):
            shape = [8, 9, 6]
            x = paddle.static.data(shape=shape, dtype='float32', name='x')
            mask = paddle.static.data(shape=shape, dtype='bool', name='mask')
            mask_float = paddle.static.data(
                shape=shape, dtype='float32', name='mask_float'
            )
            np_x = np.random.random(shape).astype('float32')
            np_mask = np.array(np.random.randint(2, size=shape, dtype=bool))

            def test_x_type():
                paddle.masked_select(np_x, mask)

            self.assertRaises(TypeError, test_x_type)

            def test_mask_type():
                paddle.masked_select(x, np_mask)

            self.assertRaises(TypeError, test_mask_type)

            def test_mask_dtype():
                paddle.masked_select(x, mask_float)

            self.assertRaises(TypeError, test_mask_dtype)


if __name__ == '__main__':
    paddle.enable_static()
    unittest.main()
