// Copyright (c) 2018 PaddlePaddle Authors. All Rights Reserved.
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

#include "paddle/fluid/inference/analysis/passes/memory_optimize_pass.h"

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "glog/logging.h"
#include "paddle/fluid/framework/ir/graph_helper.h"
#include "paddle/fluid/inference/analysis/pass_result_info.h"
#include "paddle/fluid/platform/enforce.h"

namespace paddle {
namespace framework {
namespace ir {
class Graph;
class Node;
}  // namespace ir
}  // namespace framework
}  // namespace paddle

namespace paddle {
namespace inference {
namespace analysis {

using framework::ir::Graph;
using framework::ir::Node;
using framework::ir::TopologyVarientSort;
using space_table_t = MemoryOptimizePass::space_table_t;

typedef struct {
  std::string name;
  size_t size;
  int cluster;
  std::pair<int, int> lifetime;
  std::unordered_set<std::string> adj;
} MemNode;

// Collect the lifecycles of the tensors.
// Traverse the graph in topological order.
// The traversal order also affect the lifecycles, so different sort_kind is
// used.
void MemoryOptimizePass::CollectLifeCycle(
    Graph* graph,
    std::unordered_map<std::string, lifecycle_t>* lifecycles,
    int sort_kind) const {
  int max_lifecycle = 0;
  double persis_byte = 0;
  for (auto* op_node : framework::ir::TopologyVarientSort(
           *graph, static_cast<framework::ir::SortKind>(sort_kind))) {
    if (!op_node->IsOp()) continue;
    auto reads = op_node->inputs;
    auto writes = op_node->outputs;

    std::vector<Node*>
    requires(reads.begin(), reads.end());
    requires.insert(requires.end(), writes.begin(), writes.end());

    // Disable reuse of feed variables.
    if (op_node->Name() == "feed") {
      for (auto* node : op_node->outputs) {
        auto var = node->Name();
        lifecycles->emplace(var,
                            std::make_pair(0, std::numeric_limits<int>::max()));
      }
    } else {
      // Normal operators.
      for (const Node* node : requires) {
        if (!node->Var()) continue;
        if (node->Var()->Persistable()) {
          // "Getting 'tensor_desc' is not supported by the fetch type
          // variable."
          bool is_break = false;
          for (auto op_op : node->inputs) {
            if (op_op->Name() == "fetch") is_break = true;
          }
          if (is_break) continue;

          auto in_shape = node->Var()->GetShape();
          for (auto i : in_shape) {
            CHECK_GT(i, 0);
          }
          auto var_bytes = std::accumulate(in_shape.begin(),
                                           in_shape.end(),
                                           (int64_t)1,
                                           std::multiplies<int64_t>());
          persis_byte +=
              paddle::framework::SizeOfType(node->Var()->GetDataType()) *
              var_bytes;
          continue;
        }
        std::string var = node->Name();
        if (!lifecycles->count(var)) {
          (*lifecycles)[var] = std::make_pair(max_lifecycle, max_lifecycle);
        } else {
          (*lifecycles)[var].second =
              std::max(max_lifecycle, lifecycles->at(var).second);  // max()
        }
      }
    }

    ++max_lifecycle;
  }
  LOG(INFO) << "The persistable params in main graph are : "
            << (persis_byte / (1 << 20)) << "MB";
}

void MemoryOptimizePass::CollectVarMemorySize(
    Graph* graph, space_table_t* space_table) const {
  const int fake_batch_size = 1;

  auto valid_var = [&](framework::ir::Node* node) -> bool {
    // lod operator reuse may cause unknown errors.
    std::set<std::string> invalid_op = {"while",
                                        "conditional_block",
                                        "tensorrt_engine",
                                        "conditional_block_infer",
                                        "merge_lod_tensor_infer",
                                        "merge_lod_tensor",
                                        "equal",
                                        "sequence_pool",
                                        "recurrent",
                                        "lod_reset",
                                        "fetch",
                                        "share_data"};
    for (auto* tmp : node->inputs) {
      CHECK(tmp->IsOp());
      std::string op_type = tmp->Op()->Type();
      if (std::find(invalid_op.begin(), invalid_op.end(), op_type) !=
          invalid_op.end()) {
        return false;
      }
    }
    for (auto* tmp : node->outputs) {
      CHECK(tmp->IsOp());
      std::string op_type = tmp->Op()->Type();
      if (std::find(invalid_op.begin(), invalid_op.end(), op_type) !=
          invalid_op.end()) {
        return false;
      }
    }
    return true;
  };

  // MemoryOptimizePass surppose input model is directed acyclic graph
  // although it's not always the case. so black list is the best compromise
  // between performance and underlying principle.
  std::unordered_set<std::string> black_list;
  for (auto* node : graph->Nodes()) {
    if (node->IsVar() && node->Var() &&
        node->Var()->GetType() ==
            framework::proto::VarType::Type::VarType_Type_LOD_TENSOR) {
      if (!valid_var(node)) {
        black_list.emplace(node->Var()->Name());
      }
    }
  }

  // Collect tensors from graph.
  for (auto* node : graph->Nodes()) {
    if (node->IsVar() && node->Var() &&
        node->Var()->GetType() ==
            framework::proto::VarType::Type::VarType_Type_LOD_TENSOR &&
        !black_list.count(node->Var()->Name())) {
      // Parameters will not be reused.
      if (node->Var()->Persistable()) continue;
      auto shape = node->Var()->GetShape();
      for (auto& v : shape) {
        if (v < 0) v = fake_batch_size;
      }

      int size = std::accumulate(
          shape.begin(), shape.end(), 1, std::multiplies<int>());
      (*space_table)[node->Var()->Name()] =
          size * paddle::framework::SizeOfType(node->Var()->GetDataType());
    }
  }
}

void MakeSimpleReusePlan(
    const std::unordered_map<std::string, std::pair<int, int>>& lifecycles,
    const std::unordered_map<std::string, size_t>& space_table,
    std::unordered_map<std::string, std::string>* node2cluster,
    std::unordered_map<std::string, int>* cluster_size) {
  std::vector<MemNode> mem_nodes;
  for (auto& data : lifecycles) {
    if (!space_table.count(data.first)) continue;
    MemNode temp_node;
    temp_node.name = data.first;
    temp_node.size = space_table.at(data.first);
    temp_node.cluster = -1;
    temp_node.lifetime = data.second;
    mem_nodes.push_back(temp_node);
  }
  auto overlap = [](std::pair<int, int> a, std::pair<int, int> b) -> bool {
    return b.second >= a.first && a.second >= b.first;
  };
  // If the lifetime of two nodes is overwritten, we set them as adjacent nodes.
  for (size_t i = 0; i < mem_nodes.size(); i++) {
    for (size_t j = i + 1; j < mem_nodes.size(); j++) {
      if (overlap(mem_nodes[i].lifetime, mem_nodes[j].lifetime)) {
        mem_nodes[i].adj.insert(mem_nodes[j].name);
        mem_nodes[j].adj.insert(mem_nodes[i].name);
      }
    }
  }

  // Sort the nodes according to the node memory size.
  auto sort_func = [](MemNode a, MemNode b) { return a.size > b.size; };
  std::sort(mem_nodes.begin(), mem_nodes.end(), sort_func);

  // Generating Memory Reuse Strategy Based on Greedy Way
  for (size_t i = 0; i < mem_nodes.size(); i++) {
    if (mem_nodes[i].cluster >= 0) continue;
    int cluster_index = cluster_size->size();
    mem_nodes[i].cluster = cluster_index;
    (*cluster_size)[mem_nodes[i].name] = mem_nodes[i].size;
    (*node2cluster)[mem_nodes[i].name] = mem_nodes[i].name;
    std::unordered_set<std::string> cluster_adj = mem_nodes[i].adj;
    for (size_t j = i + 1; j < mem_nodes.size(); j++) {
      if (mem_nodes[j].cluster < 0 &&
          (cluster_adj.find(mem_nodes[j].name) == cluster_adj.end())) {
        (*node2cluster)[mem_nodes[j].name] = mem_nodes[i].name;
        mem_nodes[j].cluster = cluster_index;
        for (auto& n : mem_nodes[j].adj) {
          cluster_adj.insert(n);
        }
      }
    }
  }
  for (auto& cluster : *cluster_size) {
    LOG(INFO) << "Cluster name : " << cluster.first
              << "  size: " << cluster.second;
  }
}

// Remove the inplace operation from the plan because it does not support memory
// reuse
void DelInplaceOpFromPlan(
    Graph* graph,
    std::unordered_map<std::string, std::string>* node2cluster,
    int sort_kind) {
  auto topo_nodes = TopologyVarientSort(
      *graph, static_cast<framework::ir::SortKind>(sort_kind));
  for (auto* op_node : topo_nodes) {
    if (!op_node->IsOp()) continue;
    auto input_tensors = op_node->inputs;
    auto output_tensors = op_node->outputs;

    std::unordered_set<std::string> in_names;
    for (const Node* node : input_tensors) {
      if (!node->Var()) continue;
      if (node->Var()->Persistable()) continue;
      std::string var = node->Name();
      in_names.insert(var);
    }

    for (const Node* node : output_tensors) {
      if (!node->Var()) continue;
      if (node->Var()->Persistable()) continue;
      std::string var = node->Name();
      if (in_names.find(var) != in_names.end()) {
        // delete key
        if (node2cluster->count(var)) {
          node2cluster->erase(var);
        }
        // delete value
        std::string tmp_name = "";
        for (auto it = node2cluster->begin(); it != node2cluster->end(); ++it) {
          if (it->second == var) {
            if (tmp_name == "") {
              tmp_name = it->first;
            }
            it->second = tmp_name;
          }
        }
      }
    }
  }
}

// NOTE The optimized opdesc doesn't match ir::Graph.
void UpdateOpDescsByReuse(
    Graph* graph,
    const std::unordered_map<std::string, std::string>& reuse_table,
    int sort_kind) {
  // TODO(Superjomn) change here to be compatible with the runtime order.
  for (auto* node : TopologyVarientSort(
           *graph, static_cast<framework::ir::SortKind>(sort_kind))) {
    if (node->IsOp()) {
      // Replace the original inputs/outputs with the reused tensors.
      std::unordered_map<std::string, std::vector<std::string>> in_args,
          out_args;
      for (auto argument : node->Op()->Inputs()) {
        for (const auto& x : argument.second) {
          auto name = x;
          if (reuse_table.count(x) && reuse_table.at(x) != x) {
            name = reuse_table.at(x);
          }
          in_args[argument.first].push_back(name);
          VLOG(4) << node->Name() << " input " << x << " -> " << name;
        }
      }

      // modify the graph
      for (auto input_node : node->inputs) {
        PADDLE_ENFORCE_EQ(input_node->IsVar(),
                          true,
                          platform::errors::PreconditionNotMet(
                              "The input node should be a variable."));
        std::string input_node_name = input_node->Name();
        if (reuse_table.count(input_node_name) &&
            reuse_table.at(input_node_name) != input_node_name) {
          auto name = reuse_table.at(input_node_name);
          input_node->RenameVar(name);
        }
      }

      for (auto argument : node->Op()->Outputs()) {
        for (const auto& x : argument.second) {
          auto name = x;
          if (reuse_table.count(x) && reuse_table.at(x) != x) {
            name = reuse_table.at(x);
          }
          out_args[argument.first].push_back(name);
          VLOG(4) << node->Name() << " output " << x << " -> " << name;
        }
      }

      // modify the graph
      for (auto out_node : node->outputs) {
        PADDLE_ENFORCE_EQ(out_node->IsVar(),
                          true,
                          platform::errors::PreconditionNotMet(
                              "The output node should be a variable."));
        std::string out_node_name = out_node->Name();
        if (reuse_table.count(out_node_name) &&
            reuse_table.at(out_node_name) != out_node_name) {
          auto name = reuse_table.at(out_node_name);
          out_node->RenameVar(name);
        }
      }

      // Update arguments.
      for (auto& arg : in_args) {
        node->Op()->SetInput(arg.first, arg.second);
      }
      for (auto& arg : out_args) {
        node->Op()->SetOutput(arg.first, arg.second);
      }
      node->Op()->Flush();
    }
  }
}

std::string MemoryOptimizePass::repr() const { return "memory_optimize_pass"; }

void MemoryOptimizePass::RunImpl(Argument* argument) {
  // Memory optimization.
  // We will perform the following operation:
  // 1. Collect all var's lifetime.
  // 2. Make reuse plan: the vars can be reused if there is no overlap(on
  // lifetime) between
  // them.
  // The final plan is a mapping table in which the key represents the original
  // name of var and the value in the table represents the current name of var.
  // 3. Perform reuse plan: Replace all var's name in the model according to the
  // mapping table.
  if (!argument->enable_memory_optim()) return;
  // Because of pass is a singleton, graph can not be member
  // variables, otherwise, errors will be caused under multithreading
  // conditions.
  auto graph = argument->main_graph_ptr();

  int sort_kind = 0;
  std::unordered_map<std::string, lifecycle_t> lifecycles;
  space_table_t space_table;
  std::unordered_map<std::string, std::string> node2cluster;
  std::unordered_map<std::string, int> cluster_size;

  CollectLifeCycle(graph, &lifecycles, sort_kind);
  CollectVarMemorySize(graph, &space_table);
  MakeSimpleReusePlan(lifecycles, space_table, &node2cluster, &cluster_size);
  DelInplaceOpFromPlan(graph, &node2cluster, sort_kind);

  auto* pass_res_info = PassResultInfoForRuntime::Instance();
  pass_res_info->Set(
      argument->root_predictor_id(), "memory_optimize_pass", node2cluster);

  return;
}

}  // namespace analysis
}  // namespace inference
}  // namespace paddle
