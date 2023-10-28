/**
 * Copyright 2022 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "plugin/device/ascend/optimizer/ge/avg_pool_grad_for_ge.h"

#include <string>
#include <vector>
#include <algorithm>
#include <memory>
#include "ops/conv_pool_ops.h"
#include "ops/array_ops.h"

namespace mindspore {
namespace opt {

const BaseRef AvgPoolGradForGE::DefinePattern() const {
  auto x = std::make_shared<SeqVar>();
  return VectorRef({prim::kPrimAvgPoolGrad, x});
}

const AnfNodePtr AvgPoolGradForGE::Process(const FuncGraphPtr &graph, const AnfNodePtr &node, const EquivPtr &) const {
  MS_EXCEPTION_IF_NULL(graph);
  MS_EXCEPTION_IF_NULL(node);
  auto avg_pool_grad_node = node->cast<CNodePtr>();
  MS_EXCEPTION_IF_NULL(avg_pool_grad_node);

  auto input_x = avg_pool_grad_node->input(kIndex1);
  auto input_x_shape = input_x->Shape();
  AnfNodePtr shape_node = nullptr;
  if (input_x_shape->IsDynamic()) {
    shape_node = CreateTensorShapeNode(graph, input_x);
  } else {
    auto shape_vector = input_x_shape->cast<abstract::ShapePtr>()->shape();
    std::vector<int32_t> value_node_data;
    (void)std::transform(shape_vector.begin(), shape_vector.end(), std::back_inserter(value_node_data), LongToInt);
    auto shape_tensor = std::make_shared<tensor::Tensor>(value_node_data, TypeIdToType(TypeId::kNumberTypeInt64));
    auto shape_value = MakeValue(shape_tensor);
    shape_node = NewValueNode(shape_value);
    shape_node->set_abstract(shape_value->ToAbstract());
  }
  auto manager = graph->manager();
  MS_EXCEPTION_IF_NULL(manager);
  manager->SetEdge(avg_pool_grad_node, kIndex1, shape_node);
  return avg_pool_grad_node;
}

CNodePtr AvgPoolGradForGE::CreateTensorShapeNode(const FuncGraphPtr &func_graph, const AnfNodePtr &node) const {
  auto prim = std::make_shared<Primitive>(kTensorShapeOpName);
  MS_EXCEPTION_IF_NULL(prim);
  AnfNodePtrList inputs = {NewValueNode(prim), node};
  CNodePtr tensor_shape_node = func_graph->NewCNode(inputs);
  MS_EXCEPTION_IF_NULL(tensor_shape_node);
  auto abs = InferAbstract(prim, {node});
  MS_EXCEPTION_IF_NULL(abs);
  tensor_shape_node->set_abstract(abs);
  return tensor_shape_node;
}
}  // namespace opt
}  // namespace mindspore
