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

#pragma once

#include "paddle/fluid/framework/variable.h"
#include "paddle/ir/core/dialect.h"
#include "paddle/ir/core/parameter.h"

namespace paddle {
namespace dialect {
class ParameterConvertInterface
    : public ir::DialectInterface::Base<ParameterConvertInterface> {
 public:
  explicit ParameterConvertInterface(ir::Dialect* dialect) : Base(dialect) {}

  // NOTE(zhangbo): Only support new a CPU Variable.
  std::shared_ptr<paddle::framework::Variable> ParameterToVariable(
      ir::Parameter* parameter);

  std::unique_ptr<ir::Parameter> VariableToParameter(
      paddle::framework::Variable* var);
};

class PaddleDialect : public ir::Dialect {
 public:
  explicit PaddleDialect(ir::IrContext* context);

  static const char* name() { return "pd"; }

  void PrintType(ir::Type type, std::ostream& os) const;
  void PrintAttribute(ir::Attribute type, std::ostream& os) const;

 private:
  void initialize();
};

}  // namespace dialect
}  // namespace paddle

IR_DECLARE_EXPLICIT_TYPE_ID(paddle::dialect::PaddleDialect)
IR_DECLARE_EXPLICIT_TYPE_ID(paddle::dialect::ParameterConvertInterface)
