/**
 * Copyright 2019 Huawei Technologies Co., Ltd
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

#include "src/runtime/kernel/opencl/kernel/concat.h"
#include <cstring>
#include <string>
#include <algorithm>
#include <set>
#include "src/kernel_registry.h"
#include "src/runtime/kernel/opencl/utils.h"
#include "src/runtime/kernel/opencl/cl/concat.cl.inc"

using mindspore::kernel::KERNEL_ARCH::kGPU;
using mindspore::lite::KernelRegistrar;
using mindspore::lite::RET_ERROR;
using mindspore::lite::RET_OK;
using mindspore::schema::PrimitiveType_Concat;

namespace mindspore::kernel {

int ConcatOpenCLKernel::RunAxis0() {
  auto allocator_ = ocl_runtime_->GetAllocator();
  std::vector<size_t> img_size;
  auto dst_data = out_tensors_[0]->data_c();
  auto dst_origin = cl::array<cl::size_type, 3U>{0, 0, 0};
  cl::Image2D *out_image = reinterpret_cast<cl::Image2D *>(allocator_->GetImage(dst_data));
  for (int i = 0; i < in_tensors_.size(); i++) {
    auto src_data = inputs_weight_ptrs_.at(i) == nullptr ? in_tensors_[i]->data_c() : inputs_weight_ptrs_.at(i);
    allocator_->GetImageSize(src_data, &img_size);
    auto src_origin = cl::array<cl::size_type, 3U>{0, 0, 0};
    auto region = cl::array<cl::size_type, 3U>{img_size[0], img_size[1], 1};
    cl::Image2D *input_image = reinterpret_cast<cl::Image2D *>(allocator_->GetImage(src_data));
    ocl_runtime_->GetDefaultCommandQueue()->enqueueCopyImage(*input_image, *out_image, src_origin, dst_origin, region);
    dst_origin[1] += region[1];
  }
  return RET_OK;
}

void ConcatGetWorkGroup(const std::vector<size_t> &global, std::vector<size_t> *local, int max_size) {
  const int max_divider = 8;
  const int max_x = 2, max_y = 8;
  int x = std::min(GetMaxDivisorStrategy1(global[0], max_divider), max_x);
  int yz = max_size / x;
  int y = std::min(std::min(GetMaxDivisorStrategy1(global[1], max_divider), yz), max_y);
  int z = std::min(yz / y, static_cast<int>(UP_DIV(global[2], 2)));

  local->clear();
  local->push_back(x);
  local->push_back(y);
  local->push_back(z);
}

int ConcatOpenCLKernel::CheckSpecs() {
  if ((in_tensors_.size() < 2 || in_tensors_.size() > 6) || out_tensors_.size() != 1) {
    MS_LOG(ERROR) << "in size: " << in_tensors_.size() << ", out size: " << out_tensors_.size();
    return RET_ERROR;
  }
  auto param = reinterpret_cast<ConcatParameter *>(this->op_parameter_);
  auto out_tensors_shape_size = out_tensors_[0]->shape().size();
  MS_LOG(DEBUG) << " concat at axis=:  " << param->axis_;
  if (out_tensors_shape_size > 4) {
    MS_LOG(ERROR) << " GPU Unsupported shape.size > 4 ";
    return RET_ERROR;
  }
  for (int i = 0; i < in_tensors_.size(); ++i) {
    auto in_tensors_shape_size = in_tensors_[i]->shape().size();
    if (in_tensors_shape_size > 4) {
      MS_LOG(ERROR) << " GPU Unsupported in_tensor shape.size > 4 ";
      return RET_ERROR;
    }
  }
  axis_ = param->axis_;
  if (axis_ < 0) {
    axis_ += in_tensors_.front()->shape().size();
  }
  if (axis_ < 0 || axis_ > 3) {
    MS_LOG(ERROR) << " only support axis >= 0 and axis <= 3 ";
    return RET_ERROR;
  }
  if (out_tensors_shape_size < 4 && Type() == PrimitiveType_Concat && axis_ != 0) {
    if (out_tensors_shape_size == 2) {
      axis_ = axis_ + 2;
    } else if (out_tensors_shape_size == 3) {
      axis_ = axis_ + 1;
    } else {
      MS_LOG(ERROR) << " Unsupported axis =:  " << axis_ << "  shape().size()=:  " << out_tensors_shape_size;
      return RET_ERROR;
    }
  }
  if (in_tensors_.size() < 2 || in_tensors_.size() > 6) {
    MS_LOG(ERROR) << "unsupported input size :" << in_tensors_.size();
    return RET_ERROR;
  }
  return RET_OK;
}

void ConcatOpenCLKernel::SetConstArgs() {
  GpuTensorInfo img_info(out_tensors_[0]);
  size_t dtype = enable_fp16_ ? sizeof(cl_half) : sizeof(cl_float);
  stride_w = img_info.RowPitch() / dtype;
  cl_int4 output_shape_ = {};
  for (int i = 0; i < out_tensors_[0]->shape().size(); ++i) {
    output_shape_.s[i] = out_tensors_[0]->shape()[i];
  }
  Broadcast2GpuShape(out_shape_.s, output_shape_.s, out_tensors_[0]->shape().size(), 1);
  int arg_cn = in_tensors_.size() + 1;
  if (axis_ == 3 && !Align_) {
    for (int i = 0; i < in_tensors_.size(); ++i) {
      cl_int4 temp = {};
      for (int j = 0; j < in_tensors_[i]->shape().size(); ++j) {
        temp.s[j] = in_tensors_[i]->shape()[j];
      }
      Broadcast2GpuShape(in_shape_.s, temp.s, in_tensors_[i]->shape().size(), 1);
      ocl_runtime_->SetKernelArg(kernel_, arg_cn++, in_shape_);
    }
    ocl_runtime_->SetKernelArg(kernel_, arg_cn++, stride_w);
  } else {
    for (int i = 0; i < in_tensors_.size(); ++i) {
      cl_int4 temp = {};
      for (int j = 0; j < in_tensors_[i]->shape().size(); ++j) {
        temp.s[j] = in_tensors_[i]->shape()[j];
      }
      Broadcast2GpuShape(in_shape_.s, temp.s, in_tensors_[i]->shape().size(), 1);
      in_shape_.s[3] = UP_DIV(in_shape_.s[3], C4NUM);
      ocl_runtime_->SetKernelArg(kernel_, arg_cn++, in_shape_);
    }
  }
  out_shape_.s[3] = UP_DIV(out_shape_.s[3], C4NUM);
  ocl_runtime_->SetKernelArg(kernel_, arg_cn++, out_shape_);
}

void ConcatOpenCLKernel::SetGlobalLocal() {
  const std::vector<size_t> &max_global = ocl_runtime_->GetWorkItemSize();
  if (axis_ == 3 && !Align_) {
    OH = out_shape_.s[0] * out_shape_.s[1];
    OW = out_shape_.s[2];
    global_size_ = {OH, OW, 1};
    local_size_ = {1, 1, 1};
  } else {
    OH = out_shape_.s[0] * out_shape_.s[1];
    OW = out_shape_.s[2];
    OC = out_shape_.s[3];
    global_size_ = {OH, OW, OC};
    local_size_ = {1, 1, 1};
  }
  ConcatGetWorkGroup(global_size_, &local_size_, max_global[0]);
  OpenCLKernel::AlignGlobalLocal(global_size_, local_size_);
}

int ConcatOpenCLKernel::ConvertWeightToTensor(const std::vector<lite::Tensor *> &in_tensors,
                                              std::vector<void *> *inputs_weight_ptrs, bool fp16_enable,
                                              size_t data_size) {
  for (auto in_tensor_ : in_tensors) {
    auto nhwc_shape = GetNHWCShape(in_tensor_->shape());
    if (!in_tensor_->IsConst()) {
      (*inputs_weight_ptrs).push_back(nullptr);
    } else {
      auto allocator = ocl_runtime_->GetAllocator();
      std::vector<size_t> img_size = GetImage2dShapeFromNHWC(nhwc_shape, schema::Format_NHWC4);
      int pack_weight_size = img_size[0] * img_size[1] * C4NUM;
      int plane = nhwc_shape[1] * nhwc_shape[2];
      int channel = nhwc_shape[3];
      int batch = nhwc_shape[0];
      img_size.push_back(fp16_enable ? CL_HALF_FLOAT : CL_FLOAT);
      if (!fp16_enable) {
        float *weight = new (std::nothrow) float[pack_weight_size];
        if (weight == nullptr) {
          MS_LOG(ERROR) << "Malloc buffer failed!";
          return RET_ERROR;
        }
        memset(weight, 0x00, pack_weight_size * data_size);
        if (in_tensor_->data_type() == kNumberTypeFloat32) {
          std::function<float(float)> to_dtype = [](float x) -> float { return x; };
          PackNHWCToNHWC4<float, float>(in_tensor_->data_c(), weight, batch, plane, channel, to_dtype);
        } else if (in_tensor_->data_type() == kNumberTypeFloat16) {
          std::function<float(float16_t)> to_dtype = [](float16_t x) -> float { return static_cast<float>(x); };
          PackNHWCToNHWC4<float16_t, float>(in_tensor_->data_c(), weight, batch, plane, channel, to_dtype);
        }
        if (batch * plane * channel == 1) {
          // scalar
          weight[3] = weight[2] = weight[1] = weight[0];
        }
        auto weight_ptr_ = allocator->Malloc(pack_weight_size, img_size, weight);
        (*inputs_weight_ptrs).push_back(weight_ptr_);
        delete[] weight;
      } else {
        float16_t *weight = new (std::nothrow) float16_t[pack_weight_size];
        if (weight == nullptr) {
          MS_LOG(ERROR) << "Malloc buffer failed!";
          return RET_ERROR;
        }
        memset(weight, 0x00, pack_weight_size * data_size);
        if (in_tensor_->data_type() == kNumberTypeFloat32) {
          std::function<float16_t(float)> to_dtype = [](float x) -> float16_t { return static_cast<float16_t>(x); };
          PackNHWCToNHWC4<float, float16_t>(in_tensor_->data_c(), weight, batch, plane, channel, to_dtype);
        } else if (in_tensor_->data_type() == kNumberTypeFloat16) {
          std::function<float16_t(float16_t)> to_dtype = [](float16_t x) -> float16_t { return x; };
          PackNHWCToNHWC4<float16_t, float16_t>(in_tensor_->data_c(), weight, batch, plane, channel, to_dtype);
        }
        if (batch * plane * channel == 1) {
          // scalar
          weight[3] = weight[2] = weight[1] = weight[0];
        }
        auto weight_ptr_ = allocator->Malloc(pack_weight_size, img_size, weight);
        (*inputs_weight_ptrs).push_back(weight_ptr_);
        delete[] weight;
      }
    }
  }
  return RET_OK;
}

int ConcatOpenCLKernel::Prepare() {
  enable_fp16_ = ocl_runtime_->GetFp16Enable();
  auto data_size = enable_fp16_ ? sizeof(float16_t) : sizeof(float);
  ConvertWeightToTensor(in_tensors_, &inputs_weight_ptrs_, enable_fp16_, data_size);
  if (axis_ == 0) {
    for (int i = 0; i < in_tensors_.size(); ++i) {
      if (in_tensors_.at(i)->shape().size() != 1) {
        return RET_OK;
      }
    }
    axis_ = 3;
  }
  for (int i = 0; i < in_tensors_.size(); ++i) {
    int length = in_tensors_[0]->shape().size();
    if (in_tensors_[i]->shape()[length - 1] % C4NUM != 0) {
      Align_ = false;
    }
  }

  std::string kernel_name = "Concat";
  if (axis_ == 3 && !Align_) {
    kernel_name += "Input" + std::to_string(in_tensors_.size()) + "UnAlign";
  } else {
    kernel_name += std::to_string(in_tensors_.size()) + "inputaxis" + std::to_string(axis_);
  }

  kernel_name += "_NHWC4";
  MS_LOG(DEBUG) << "kernel_name=: " << kernel_name;
  std::string source = concat_source;
  std::string program_name = "Concat";
  ocl_runtime_->LoadSource(program_name, source);
  ocl_runtime_->BuildKernel(kernel_, program_name, kernel_name);
  MS_LOG(DEBUG) << kernel_name << " Init Done!";
  SetConstArgs();
  SetGlobalLocal();
  return RET_OK;
}

int ConcatOpenCLKernel::Run() {
  MS_LOG(DEBUG) << this->name() << " Running! ";
  if (axis_ == 0) {
    return RunAxis0();
  }
  int arg_cn = 0;
  for (int i = 0; i < in_tensors_.size(); ++i) {
    auto input_ptr = inputs_weight_ptrs_.at(i) == nullptr ? in_tensors_[i]->data_c() : inputs_weight_ptrs_.at(i);
    ocl_runtime_->SetKernelArg(kernel_, arg_cn++, input_ptr);
  }
  if (axis_ == 3 && !Align_) {
    ocl_runtime_->SetKernelArg(kernel_, arg_cn++, out_tensors_[0]->data_c(), lite::opencl::MemType::BUF);
  } else {
    ocl_runtime_->SetKernelArg(kernel_, arg_cn++, out_tensors_[0]->data_c());
  }
  ocl_runtime_->RunKernel(kernel_, global_range_, local_range_, nullptr, &event_);
  return RET_OK;
}

REG_KERNEL(kGPU, kNumberTypeFloat32, PrimitiveType_Concat, OpenCLKernelCreator<ConcatOpenCLKernel>)
REG_KERNEL(kGPU, kNumberTypeFloat16, PrimitiveType_Concat, OpenCLKernelCreator<ConcatOpenCLKernel>)
}  // namespace mindspore::kernel
