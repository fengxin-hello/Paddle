// Copyright (c) 2023 PaddlePaddle Authors. All Rights Reserved.
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

#include "paddle/fluid/ir/dialect/kernel_dialect.h"
#include "paddle/fluid/ir/dialect/kernel_op.h"
#include "paddle/fluid/ir/dialect/pd_attribute.h"
// NOTE(zhangbo9674): File pd_op.h is generated by op_gen.py, see details in
// paddle/fluid/ir/dialect/CMakeLists.txt.
#include "paddle/fluid/framework/convert_utils.h"
#include "paddle/fluid/framework/data_type.h"
#include "paddle/fluid/ir/dialect/kernel_attribute.h"
#include "paddle/fluid/ir/dialect/kernel_type.h"
#include "paddle/fluid/ir/dialect/kernel_type_storage.h"
#include "paddle/fluid/ir/dialect/pd_op.h"
#include "paddle/fluid/ir/dialect/utils.h"
#include "paddle/fluid/platform/init_phi.h"
#include "paddle/ir/core/dialect_interface.h"
#include "paddle/phi/core/dense_tensor.h"

REGISTER_FILE_SYMBOLS(kernel_dialect);

namespace paddle {
namespace dialect {

PaddleKernelDialect::PaddleKernelDialect(ir::IrContext *context)
    : ir::Dialect(name(), context, ir::TypeId::get<PaddleKernelDialect>()) {
  initialize();
}

void PaddleKernelDialect::initialize() {
  RegisterTypes<paddle::dialect::AllocatedDenseTensorType>();
  RegisterOps<dialect::PhiKernelOp>();

  RegisterAttributes<paddle::dialect::KernelAttribute>();
}

void PaddleKernelDialect::PrintType(ir::Type type, std::ostream &os) const {
  AllocatedDenseTensorType tensor_type =
      type.dyn_cast<AllocatedDenseTensorType>();

  os << phi::AllocationTypeStr(tensor_type.place().GetType()) << "_";
  os << "tensor<";
  for (auto d : phi::vectorize(tensor_type.dims())) {
    os << d;
    os << "x";
  }
  tensor_type.dtype().Print(os);
  os << ">";
}

void PaddleKernelDialect::PrintAttribute(ir::Attribute attr,
                                         std::ostream &os) const {
  phi::KernelKey kernel = attr.dyn_cast<KernelAttribute>().data();

  os << "<backend:" << kernel.backend() << "|layout:" << kernel.layout()
     << "|dtype:" << kernel.dtype() << ">";
}

}  // namespace dialect
}  // namespace paddle

IR_DEFINE_EXPLICIT_TYPE_ID(paddle::dialect::PaddleKernelDialect)
