// Copyright (c) 2022 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "paddle/phi/backends/gpu/gpu_context.h"
#ifndef PADDLE_WITH_XPU_KP
#include "paddle/phi/common/complex.h"
#include "paddle/phi/common/float16.h"
#endif
#include "paddle/phi/core/kernel_registry.h"
#include "paddle/phi/kernels/elementwise_add_kernel.h"
#include "paddle/phi/kernels/impl/elementwise_kernel_impl.h"

namespace phi {

template <typename T, typename Context>
void AddCudaFunctor(const Context& dev_ctx,
                    const DenseTensor& x,
                    const DenseTensor& y,
                    int axis,
                    DenseTensor* out) {
  std::vector<const DenseTensor*> inputs;
  inputs.reserve(2);
  std::vector<DenseTensor*> outputs;
  outputs.reserve(1);
  inputs.emplace_back(&x);
  inputs.emplace_back(&y);
  outputs.emplace_back(out);
  dev_ctx.template Alloc<T>(out);
  funcs::BroadcastKernel<T>(
      dev_ctx, inputs, &outputs, funcs::AddFunctor<T>(), axis);
}

template <typename T, typename Context>
void Float32Bfloat16OrFloat16AddCudaFunctor(const Context& dev_ctx,
                                            const DenseTensor& x,
                                            const DenseTensor& y,
                                            DenseTensor* out) {
  std::vector<const DenseTensor*> inputs;
  inputs.reserve(2);
  std::vector<DenseTensor*> outputs;
  outputs.reserve(1);
  inputs.emplace_back(&x);
  inputs.emplace_back(&y);
  outputs.emplace_back(out);
  if (y.dtype() == phi::DataType::BFLOAT16) {
    funcs::ElementwiseKernel<T>(
        dev_ctx, inputs, &outputs, funcs::Float32Bfloat16AddFunctor<T>());
  } else if (y.dtype() == phi::DataType::FLOAT16) {
    funcs::ElementwiseKernel<T>(
        dev_ctx, inputs, &outputs, funcs::Float32Float16AddFunctor<T>());
  } else {
    PADDLE_THROW(phi::errors::InvalidArgument(
        "Unsupport x dtype:%s, y dtype:%s for add(x, y) operation",
        phi::DataTypeToString(x.type()),
        phi::DataTypeToString(y.type())));
  }
}

template <typename T, typename Context>
void AddKernel(const Context& dev_ctx,
               const DenseTensor& x,
               const DenseTensor& y,
               DenseTensor* out) {
  if (x.dtype() == phi::DataType::FLOAT32 &&
      (y.dtype() == phi::DataType::BFLOAT16 ||
       y.dtype() == phi::DataType::FLOAT16)) {
    using Type = DataTypeToCppType<phi::DataType::FLOAT32>::type;
    Float32Bfloat16OrFloat16AddCudaFunctor<Type, Context>(dev_ctx, x, y, out);
  } else {
    AddCudaFunctor<T, Context>(dev_ctx, x, y, -1, out);
  }
}

template <typename T, typename Context>
void GradAddKernel(const Context& dev_ctx,
                   const DenseTensor& x,
                   const DenseTensor& y,
                   DenseTensor* out) {
  AddCudaFunctor<T>(dev_ctx, x, y, -1, out);
}

}  // namespace phi

#ifdef PADDLE_WITH_XPU_KP
PD_REGISTER_KERNEL(add, KPS, ALL_LAYOUT, phi::AddKernel, float) {}
#else

using float16 = phi::dtype::float16;
using bfloat16 = phi::dtype::bfloat16;
using complex64 = ::phi::dtype::complex<float>;
using complex128 = ::phi::dtype::complex<double>;

PD_REGISTER_KERNEL(add,
                   KPS,
                   ALL_LAYOUT,
                   phi::AddKernel,
                   float,
                   double,
                   int16_t,
                   int,
                   int64_t,
                   phi::dtype::float16,
                   phi::dtype::bfloat16,
                   complex64,
                   complex128) {}

PD_REGISTER_KERNEL(grad_add,
                   KPS,
                   ALL_LAYOUT,
                   phi::GradAddKernel,
                   float,
                   double,
                   int16_t,
                   int,
                   int64_t,
                   phi::dtype::float16,
                   phi::dtype::bfloat16,
                   complex64,
                   complex128) {}
#endif
