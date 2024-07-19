/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_CUDA_CUDAERRORHELPER_H_
#define MCF_CUDA_CUDAERRORHELPER_H_

#include "cuda_runtime.h"
#include "mcf_cuda/CudaError.h"

// TODO: for some strange reason we have to include <memory>
//       in order to make std exceptions work with nvcc
#include <exception>
#include <memory>
#include <string>

namespace mcf
{
namespace cuda
{

// define own error macros to be self-contained
#define THROW_CUDA(error)                                                                          \
    throw mcf::cuda::cuda_error(                                                                   \
        std::string("Cuda error ") + std::to_string(error) + std::string(" in " __FILE__ ", ")     \
        + std::to_string(__LINE__))

#define CUDA_ASSERT(condition, what)                                                               \
    {                                                                                              \
        if (!(condition))                                                                          \
        {                                                                                          \
            throw std::runtime_error(                                                              \
                "Assertion failed in " __FILE__ ", " + std::to_string(__LINE__));                  \
        }                                                                                          \
    }

/**
 * Throw in case of a cuda error
 */
#define MCF_CHECK_CUDA(result)                                                                     \
    {                                                                                              \
        cudaError tmp(result);                                                                     \
        if (cudaSuccess != tmp)                                                                    \
            throw mcf::cuda::cuda_error(                                                           \
                std::string("Cuda error ") + std::to_string(tmp)                                   \
                + std::string(" in " __FILE__ ", ") + std::to_string(__LINE__));                   \
    }

#define MCF_CHECK_CUDA_ERROR                                                                       \
    {                                                                                              \
        MCF_CHECK_CUDA(cudaDeviceSynchronize());                                                   \
        MCF_CHECK_CUDA(cudaGetLastError());                                                        \
    }

// saves like 1ms per frame in execution time but also introduces random 100ms stalls between
// individual frames' processing
//#define CHECK_CUDA(result) (result)
//#define CHECK_CUDA_ERROR

} // namespace cuda
} // namespace mcf

#endif /* MCF_CUDA_CUDAERRORHELPER_H_ */
