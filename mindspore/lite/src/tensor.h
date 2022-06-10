/**
 * Copyright 2020 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_LITE_SRC_TENSOR_H_
#define MINDSPORE_LITE_SRC_TENSOR_H_

#include <math.h>
#include <memory>
#include <vector>
#include <string>
#include <numeric>
#include <functional>
#include <atomic>
#include "include/api/format.h"
#include "include/lite_types.h"
#include "src/runtime/inner_allocator.h"
#include "src/common/log_adapter.h"
#include "src/common/utils.h"
#include "src/runtime/tensor_category.h"

namespace mindspore {
namespace lite {
#define STATIC_ALLOCATION -271964
#define RUNTIME_REFCOUNT 0x9999
#define IS_STATIC_ALLOCATOR(allocator) ((allocator != nullptr) && (allocator->RefCount(nullptr) == STATIC_ALLOCATION))
#define IS_RUNTIME_ALLOCATOR(allocator) ((allocator != nullptr) && (allocator->RefCount(nullptr) == RUNTIME_REFCOUNT))
struct LiteQuantParam {
  double scale;
  int32_t zeroPoint;
  float var_corr{1};
  float mean_corr{0};
  bool inited{false};
  std::vector<float> clusters{};
  int bitNum{8};
  int roundType{1};
  int multiplier{1};
  int dstDtype{32};
  // dynamic range
  double min{-255.0};
  double max{255.0};
};

class Tensor {
 public:
  Tensor() = default;

  Tensor(TypeId data_type, std::vector<int> shape, const mindspore::Format &format = mindspore::NHWC,
         Category category = VAR);

  Tensor(const Tensor &tensor) = delete;

  Tensor(Tensor &&other) = delete;

  Tensor &operator=(const Tensor &tensor) = delete;

  Tensor &operator=(Tensor &&src) = delete;

  virtual ~Tensor();

  static Tensor *CreateTensor(const std::string &name, TypeId type, const std::vector<int> &shape, const void *data,
                              size_t data_len);
  static Tensor *CreateTensorByDeepCopy(const std::string &name, TypeId type, const std::vector<int> &shape,
                                        const void *data, size_t data_len);

  static int CopyTensorData(const Tensor &src_tensor, Tensor *dst_tensor);

  static Tensor *CopyTensor(const Tensor &src_tensor, bool copy_data = false, AllocatorPtr allocator = nullptr);

  virtual bool operator==(const Tensor &tensor);

  void set_tensor_name(const std::string &name) { tensor_name_ = name; }

  std::string tensor_name() const { return tensor_name_; }

  TypeId data_type() const { return data_type_; }

  void set_data_type(TypeId data_type) { data_type_ = data_type; }

  std::vector<int> shape() const { return shape_; }

  void set_shape(const std::vector<int> &shape) { shape_ = shape; }

  int DimensionSize(size_t index) const;

  int64_t ElementsNum() const;

  int32_t Batch() const;

  int32_t Channel() const;

  int32_t Height() const;

  int32_t Width() const;

  int64_t ElementsC4Num() const;

  int64_t ElementsC8Num() const;
  virtual size_t Size() const;

  virtual void set_allocator(AllocatorPtr allocator) { allocator_ = allocator; }

  AllocatorPtr allocator() const { return allocator_; }

  virtual int MallocData(const AllocatorPtr allocator = nullptr);

  virtual void FreeData();

  virtual void *MutableData();

  void *ReallocData();

  virtual void *data() { return data_; }

  virtual void *data() const { return data_; }

  // note: in the case of that old_data is valid, set_data just releases the ownership of it but not frees it. Of
  //       course, you can call FreeData before calling set_data to ensure the data can be freed by current tensor.
  void set_data(void *data) {
    if (this->data_ == data) {
      return;
    }
    if (allocator_ != nullptr) {
      allocator_->IncRefCount(data, 1);
      allocator_->DecRefCount(this->data_, 1);
    }
    this->data_ = data;
    this->own_data_ = true;
  }

  Category category() const { return this->category_; }

  void set_category(Category category) { this->category_ = category; }

  void set_format(mindspore::Format format) { this->format_ = format; }

  mindspore::Format format() const { return this->format_; }
  virtual int ref_count() const { return ref_count_; }

  virtual int init_ref_count() const { return static_cast<int>(this->init_ref_count_); }

  virtual void set_ref_count(int ref_count) { ref_count_ = ref_count; }

  virtual void set_init_ref_count(int ref_count) { this->init_ref_count_ = ref_count; }

  virtual void ResetRefCount() { set_ref_count(static_cast<int>(this->init_ref_count_)); }

  virtual void IncRefCount() { ++ref_count_; }

  virtual void DecRefCount();

  std::string ToString() const;

  void AddQuantParam(const LiteQuantParam &quant_param);

  void ClearQuantParam();

  std::vector<LiteQuantParam> quant_params() const;

  void set_quant_params(std::vector<LiteQuantParam>);

  std::vector<float> quant_clusters() const;

  void set_quant_clusters(const std::vector<float> &clusters);

  virtual bool IsConst() const {
    return (this->category_ == CONST_TENSOR || this->category_ == CONST_SCALAR) && this->data_ != nullptr;
  }

  bool IsScalar() const { return this->category_ == CONST_SCALAR && this->data_ != nullptr; }

  bool IsGraphInput() const { return this->category_ == GRAPH_INPUT; }

  bool IsGraphOutput() const { return this->category_ == GRAPH_OUTPUT; }

  void Prepare() {
    if (allocator_ != nullptr) {
      data_ = allocator_->Prepare(data_);
    }
  }

  bool IsReady() const {
    return this->IsConst() || (this->IsGraphInput() && this->data_ != nullptr) || ref_count() >= 1;
  }

  bool own_data() const { return this->own_data_; }

  virtual void set_own_data(bool own_data) { this->own_data_ = own_data; }

  template <typename T>
  int Scale(float scale) {
    T cast_scale = static_cast<T>(scale);
    auto data = reinterpret_cast<T *>(data_);
    if (data == nullptr) {
      return RET_ERROR;
    }
    int length = ElementsNum();
    for (int i = 0; i < length; i++) {
      data[i] *= cast_scale;
    }
    scale_ *= scale;
    return RET_OK;
  }

  float get_scale() const { return this->scale_; }

  void set_scale(float scale) { this->scale_ = scale; }

  bool IsScale() const { return (std::fabs(this->scale_ - 1.0f) > 1.0e-05); }

 private:
  template <typename T>
  std::string DataToString(void *data, size_t data_number, size_t print_len = 40) const {
    if (data == nullptr) {
      return "Data of tensor is nullptr";
    }
    std::ostringstream oss;
    auto casted_data = static_cast<T *>(data);
    for (size_t i = 0; i < print_len && i < data_number; i++) {
      oss << " " << casted_data[i];
    }
    return oss.str();
  }

 protected:
  std::string tensor_name_;
  void *data_ = nullptr;
  TypeId data_type_;
  std::vector<int> shape_;
  mindspore::Format format_;
  Category category_;
  std::atomic_int ref_count_ = {0};
  int init_ref_count_ = 0;
  std::vector<LiteQuantParam> quant_params_;
  std::vector<float> quant_clusters_;
  AllocatorPtr allocator_ = nullptr;
  bool own_data_{false};
  float scale_ = 1.0f;
};
}  // namespace lite
}  // namespace mindspore

using TensorPtr = std::shared_ptr<mindspore::lite::Tensor>;
#endif  // MINDSPORE_LITE_SRC_TENSOR_H_
