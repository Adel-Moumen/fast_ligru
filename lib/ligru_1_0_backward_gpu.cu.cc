// Copyright 2022 Adel Moumen. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ==============================================================================

#include <cublas_v2.h>
#include <cuda_runtime_api.h>

#include "blas.h"
#include "device_assert.h"
#include "inline_ops.h"
#include "ligru_1_0.h"

namespace {

template <typename T>
__global__ void PointwiseOperationsReLU(
    const int batch_dim, const int hidden_dim, const T *h, const T *v,
    T *dh_prev, const T *grad_out,
    T *dwx) {
  const int row = blockDim.x * blockIdx.x + threadIdx.x;
  const int col = blockDim.y * blockIdx.y + threadIdx.y;

  if (row >= hidden_dim || col >= batch_dim)
    return;

  const int base_idx = col * hidden_dim + row;

  T dh = grad_out[base_idx] + dh_prev[base_idx];

  const int stride3_base_idx = col * (hidden_dim * 3) + row;
  const int z_idx = stride3_base_idx + 1 * hidden_dim;
  const int a_idx = stride3_base_idx + 0 * hidden_dim;
  const int hcand_idx = stride3_base_idx + 2 * hidden_dim;

  const T z = v[z_idx];
  const T a = v[a_idx];
  const T hcand = v[hcand_idx];

  const T tmp = (static_cast<T>(1.0) - z) * dh;
  const T dat = d_relu(a) * tmp;
  const T dzt = (h[base_idx] - hcand) * z * tmp;

  dh_prev[base_idx] = dh * z;

  const int idx = col * (hidden_dim * 2) + row;
  dwx[idx + 1 * hidden_dim] = dzt;
  dwx[idx + 0 * hidden_dim] = dat;
}

template <typename T>
__global__ void PointwiseOperationsLeakyReLU(
    const int batch_dim, const int hidden_dim, const T *h, const T *v,
    T *dh_prev, const T *grad_out,
    T *dwx) { 
  const int row = blockDim.x * blockIdx.x + threadIdx.x;
  const int col = blockDim.y * blockIdx.y + threadIdx.y;

  if (row >= hidden_dim || col >= batch_dim)
    return;

  const int base_idx = col * hidden_dim + row;

  T dh = grad_out[base_idx] + dh_prev[base_idx];

  const int stride3_base_idx = col * (hidden_dim * 3) + row;
  const int z_idx = stride3_base_idx + 1 * hidden_dim;
  const int a_idx = stride3_base_idx + 0 * hidden_dim;
  const int hcand_idx = stride3_base_idx + 2 * hidden_dim;

  const T z = v[z_idx];
  const T a = v[a_idx];
  const T hcand = v[hcand_idx];

  const T tmp = (static_cast<T>(1.0) - z) * dh;
  const T dat = d_leaky_relu(a) * tmp;
  const T dzt = (h[base_idx] - hcand) * z * tmp;

  dh_prev[base_idx] = dh * z;

  const int idx = col * (hidden_dim * 2) + row;
  dwx[idx + 1 * hidden_dim] = dzt;
  dwx[idx + 0 * hidden_dim] = dat;
}

template <typename T>
__global__ void PointwiseOperationsTanh(
    const int batch_dim, const int hidden_dim, const T *h, const T *v,
    T *dh_prev, const T *grad_out,
    T *dwx) { 
  const int row = blockDim.x * blockIdx.x + threadIdx.x;
  const int col = blockDim.y * blockIdx.y + threadIdx.y;

  if (row >= hidden_dim || col >= batch_dim)
    return;

  const int base_idx = col * hidden_dim + row;

  T dh = grad_out[base_idx] + dh_prev[base_idx];

  const int stride3_base_idx = col * (hidden_dim * 3) + row;
  const int z_idx = stride3_base_idx + 1 * hidden_dim;
  const int a_idx = stride3_base_idx + 0 * hidden_dim;
  const int hcand_idx = stride3_base_idx + 2 * hidden_dim;

  const T z = v[z_idx];
  const T a = v[a_idx];
  const T hcand = v[hcand_idx];

  const T tmp = (static_cast<T>(1.0) - z) * dh;
  const T dat = d_tanh(a) * tmp;
  const T dzt = (h[base_idx] - hcand) * z * tmp;

  dh_prev[base_idx] = dh * z;

  const int idx = col * (hidden_dim * 2) + row;
  dwx[idx + 1 * hidden_dim] = dzt;
  dwx[idx + 0 * hidden_dim] = dat;
}

template <typename T>
__global__ void PointwiseOperationsSin(
    const int batch_dim, const int hidden_dim, const T *h, const T *v,
    T *dh_prev, const T *grad_out,
    T *dwx) { // Zoneout mask (only used if ApplyZoneout==true)
  const int row = blockDim.x * blockIdx.x + threadIdx.x;
  const int col = blockDim.y * blockIdx.y + threadIdx.y;

  if (row >= hidden_dim || col >= batch_dim)
    return;

  const int base_idx = col * hidden_dim + row;

  T dh = grad_out[base_idx] + dh_prev[base_idx];

  const int stride3_base_idx = col * (hidden_dim * 3) + row;
  const int z_idx = stride3_base_idx + 1 * hidden_dim;
  const int a_idx = stride3_base_idx + 0 * hidden_dim;
  const int hcand_idx = stride3_base_idx + 2 * hidden_dim;

  const T z = v[z_idx];
  const T a = v[a_idx];
  const T hcand = v[hcand_idx];

  const T tmp = (static_cast<T>(1.0) - z) * dh;
  const T dat = d_sin(a) * tmp;
  const T dzt = (h[base_idx] - hcand) * z * tmp;

  dh_prev[base_idx] = dh * z;

  const int idx = col * (hidden_dim * 2) + row;
  dwx[idx + 1 * hidden_dim] = dzt;
  dwx[idx + 0 * hidden_dim] = dat;
}

#if defined(__CUDA_ARCH__) && (__CUDA_ARCH__ < 700)
template <typename T>
__global__ void PointwiseOperationsReLU(const int batch_dim,
                                        const int hidden_dim, const half *h,
                                        const half *v, half *dh,
                                        const half *dh_new, half *dwx) {
  device_assert_fail("FP16 is not supported on compute capability < 7.0.");
}

template <typename T>
__global__ void
PointwiseOperationsLeakyReLU(const int batch_dim, const int hidden_dim,
                             const half *h, const half *v, half *dh,
                             const half *dh_new, half *dwx) {
  device_assert_fail("FP16 is not supported on compute capability < 7.0.");
}

template <typename T>
__global__ void PointwiseOperationsTanh(const int batch_dim,
                                        const int hidden_dim, const half *h,
                                        const half *v, half *dh,
                                        const half *dh_new, half *dwx) {
  device_assert_fail("FP16 is not supported on compute capability < 7.0.");
}

template <typename T>
__global__ void
PointwiseOperationsSin(const int batch_dim, const int hidden_dim, const half *h,
                       const half *v, half *dh, const half *dh_new, half *dwx) {
  device_assert_fail("FP16 is not supported on compute capability < 7.0.");
}
#endif

} // anonymous namespace

namespace haste {
namespace v0 {
namespace ligru_1_0 {

template <typename T> struct BackwardPass<T>::private_data {
  int batch_size;
  int input_size;
  int hidden_size;
  int activation;
  cublasHandle_t blas_handle;
  cudaStream_t stream[2];
  cudaEvent_t event;
  cudaStream_t sync_stream;
};

template <typename T>
BackwardPass<T>::BackwardPass(const int batch_size, const int input_size,
                              const int hidden_size,
                              const cublasHandle_t &blas_handle,
                              const int activation, const cudaStream_t &stream)
    : data_(new private_data) {

  data_->activation = activation;
  data_->batch_size = batch_size;
  data_->input_size = input_size;
  data_->hidden_size = hidden_size;
  data_->blas_handle = blas_handle;
  data_->sync_stream = stream;
  cudaStreamCreate(&data_->stream[0]);
  cudaStreamCreate(&data_->stream[1]);
  cudaEventCreateWithFlags(&data_->event, cudaEventDisableTiming);
}

template <typename T> BackwardPass<T>::~BackwardPass() {
  if (data_->sync_stream) {
    cudaEventRecord(data_->event, data_->stream[1]);
    cudaStreamWaitEvent(data_->sync_stream, data_->event, 0);
    cudaEventRecord(data_->event, data_->stream[0]);
    cudaStreamWaitEvent(data_->sync_stream, data_->event, 0);
  } else {
    cudaStreamSynchronize(data_->stream[1]);
    cudaStreamSynchronize(data_->stream[0]);
  }
  cudaEventDestroy(data_->event);
  cudaStreamDestroy(data_->stream[1]);
  cudaStreamDestroy(data_->stream[0]);
  delete data_;
}

template <typename T>
void BackwardPass<T>::IterateInternal(const T *u_t, const T *h, const T *v,
                                      const T *grad_out, T *dh, T *dwx) {
  const T alpha = static_cast<T>(1.0);
  const T beta_sum = static_cast<T>(1.0);

  const int batch_size = data_->batch_size;
  const int hidden_size = data_->hidden_size;
  const cublasHandle_t blas_handle = data_->blas_handle;
  const cudaStream_t stream1 = data_->stream[0];
  const cudaEvent_t event = data_->event;

  const dim3 blockDim(32, 16);
  const dim3 gridDim((hidden_size + blockDim.x - 1) / blockDim.x,
                     (batch_size + blockDim.y - 1) / blockDim.y);

  if (data_->activation == 0) {
    PointwiseOperationsReLU<T><<<gridDim, blockDim, 0, stream1>>>(
        batch_size, hidden_size, h, v, dh, grad_out, dwx);
    cudaEventRecord(event, stream1);
  } else if (data_->activation == 1) {
    PointwiseOperationsLeakyReLU<T><<<gridDim, blockDim, 0, stream1>>>(
        batch_size, hidden_size, h, v, dh, grad_out, dwx);
    cudaEventRecord(event, stream1);
  } else if (data_->activation == 2) {
    PointwiseOperationsSin<T><<<gridDim, blockDim, 0, stream1>>>(
        batch_size, hidden_size, h, v, dh, grad_out, dwx);
    cudaEventRecord(event, stream1);
  } else if (data_->activation == 3) {
    PointwiseOperationsTanh<T><<<gridDim, blockDim, 0, stream1>>>(
        batch_size, hidden_size, h, v, dh, grad_out, dwx);
    cudaEventRecord(event, stream1);
  }

  cublasSetStream(blas_handle, stream1);
  blas<T>::gemm(blas_handle, CUBLAS_OP_N, CUBLAS_OP_N, hidden_size, batch_size,
                hidden_size * 2, &alpha, u_t, hidden_size, dwx, hidden_size * 2,
                &beta_sum, dh, hidden_size);

  cudaStreamWaitEvent(stream1, event, 0);
};

template <typename T>
void BackwardPass<T>::Run(const int time_step, const T *wx_t, const T *u_t,
                          const T *h, const T *v, const T *grad_out, T *dwx,
                          T *du, T *dh) {

  const blas<void>::enable_tensor_cores scoped0(data_->blas_handle);
  const blas<void>::set_pointer_mode scoped1(data_->blas_handle);

  const T alpha = static_cast<T>(1.0);
  const T beta_sum = static_cast<T>(1.0);

  const int batch_size = data_->batch_size;
  const int hidden_size = data_->hidden_size;
  const cublasHandle_t blas_handle = data_->blas_handle;
  const cudaStream_t stream2 = data_->stream[1];
  const cudaEvent_t event = data_->event;

  cudaStream_t save_stream;
  cublasGetStream(blas_handle, &save_stream);

  const int NH = batch_size * hidden_size;
  for (int i = time_step - 1; i >= 0; --i) {
    IterateInternal(u_t, h + i * NH, v + i * NH * 3, grad_out + (i + 1) * NH,
                    dh, dwx + i * NH * 2);
  }

  cudaStreamWaitEvent(stream2, event, 0);

  cublasSetStream(blas_handle, stream2);
  blas<T>::gemm(blas_handle, CUBLAS_OP_N, CUBLAS_OP_T, hidden_size * 2,
                hidden_size, batch_size * time_step, &alpha, dwx,
                hidden_size * 2, h, hidden_size, &beta_sum, du,
                hidden_size * 2);

  cublasSetStream(blas_handle, save_stream);
}

template struct BackwardPass<half>;
template struct BackwardPass<float>;
template struct BackwardPass<double>;
} // namespace ligru_1_0
} // namespace v0
} // namespace haste