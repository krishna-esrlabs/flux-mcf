/**
 * Copyright (c) 2024 Accenture
 */

#ifndef MCF_CUDA_CUDACACHINGALLOCATOR_H
#define MCF_CUDA_CUDACACHINGALLOCATOR_H

#if HAVE_CUDA
#include "cuda_runtime.h"
#include "cub/util_allocator.cuh"

namespace mcf
{
namespace cuda
{
/**
 * Declaration of caching allocator for device memory
 */
extern cub::CachingDeviceAllocator gpuAllocator;
} // namespace cuda
} // namespace mcf
#endif

#endif // MCF_CUDA_CUDACACHINGALLOCATOR_H
