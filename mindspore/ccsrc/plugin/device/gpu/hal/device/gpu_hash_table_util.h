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
#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_HAL_DEVICE_GPU_HASH_TABLE_UTIL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_HAL_DEVICE_GPU_HASH_TABLE_UTIL_H_

#include "plugin/device/gpu/hal/device/gpu_hash_table.h"
#include <map>
#include <tuple>
#include <utility>
#include <string>
#include <memory>
#if CUDA_VERSION > 11000 && defined(__linux__)

namespace mindspore {
namespace device {
namespace gpu {
using SetHashTableFunc = std::function<void(const UserDataPtr &)>;
using SyncHashTableFunc = std::function<bool(const UserDataPtr &, const void *, size_t)>;
using ClearHashTableFunc = std::function<void(const UserDataPtr &)>;

template <typename KeyType, typename ValueType>
void SetHashTable(const UserDataPtr &user_data) {
  MS_EXCEPTION_IF_NULL(user_data);
  auto shape_vector = user_data->get<ShapeVector>(kHashTableShapeVector);
  auto default_value = user_data->get<Value>(kHashTableDefaultValue);
  MS_EXCEPTION_IF_NULL(shape_vector);
  MS_EXCEPTION_IF_NULL(default_value);
  int32_t value_size = 1;
  for (size_t i = 0; i < (*shape_vector).size(); ++i) {
    value_size *= (*shape_vector)[i];
  }
  if (value_size <= 0) {
    MS_LOG(WARNING) << "Invalid value size:" << value_size;
  }
  if (default_value->isa<StringImm>()) {
    user_data->set<GPUHashTable<KeyType, ValueType>>(
      kUserDataData,
      std::make_shared<GPUHashTable<KeyType, ValueType>>(value_size, GetValue<std::string>(default_value)));
  } else if (default_value->isa<FloatImm>()) {
    user_data->set<GPUHashTable<KeyType, ValueType>>(
      kUserDataData, std::make_shared<GPUHashTable<KeyType, float>>(value_size, GetValue<float>(default_value)));
  } else {
    MS_LOG(EXCEPTION) << "Invalid default value:" << default_value;
  }
}

template <typename KeyType, typename ValueType>
bool SyncHashTable(const UserDataPtr &user_data, const void *host_ptr, size_t size) {
  MS_EXCEPTION_IF_NULL(user_data);
  MS_EXCEPTION_IF_NULL(host_ptr);
  const auto &gpu_hash_table = user_data->get<GPUHashTable<KeyType, ValueType>>(kUserDataData);
  MS_EXCEPTION_IF_NULL(gpu_hash_table);
  if (!gpu_hash_table->Import({const_cast<void *>(host_ptr), size})) {
    MS_LOG(ERROR) << "Import for hash table failed.";
    return false;
  }
  return true;
}

template <typename KeyType, typename ValueType>
void ClearHashTable(const UserDataPtr &user_data) {
  MS_EXCEPTION_IF_NULL(user_data);
  const auto &user_data_data = user_data->get<GPUHashTable<KeyType, ValueType>>(kUserDataData);
  MS_EXCEPTION_IF_NULL(user_data_data);
  if (!user_data_data->Clear()) {
    MS_LOG(EXCEPTION) << "Clear user data failed.";
  }
}

static std::map<std::pair<TypeId, TypeId>, std::tuple<SetHashTableFunc, SyncHashTableFunc, ClearHashTableFunc>>
  hashtable_func_list = {
    {std::make_pair(TypeId::kNumberTypeInt32, TypeId::kNumberTypeFloat32),
     std::make_tuple(SetHashTable<int, float>, SyncHashTable<int, float>, ClearHashTable<int, float>)},
    {std::make_pair(TypeId::kNumberTypeInt64, TypeId::kNumberTypeFloat32),
     std::make_tuple(SetHashTable<int64_t, float>, SyncHashTable<int64_t, float>, ClearHashTable<int64_t, float>)}};

constexpr size_t kSetFuncIndex = 0;
constexpr size_t kSyncFuncIndex = 1;
constexpr size_t kClearFuncIndex = 2;
}  // namespace gpu
}  // namespace device
}  // namespace mindspore
#endif
#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_HAL_DEVICE_GPU_HASH_TABLE_UTIL_H_
