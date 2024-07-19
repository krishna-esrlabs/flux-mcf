/**
 * Copyright (c) 2024 Accenture
 */

#if HAVE_CUDA
#include "cuda_runtime.h"
#include "cub/util_allocator.cuh"

namespace mcf
{
namespace cuda
{

/**
 * Instantiation of CachingDeviceAllocator to be used for all
 * CUDA device memory allocations in the project by declaring
 * an extern cub::CachingDeviceAllocator gpuAllocator
 */
cub::CachingDeviceAllocator gpuAllocator(4u, 4u, 12u, 1073741824u);

} // namespace cuda
} // namespace mcf

#endif // HAVE_CUDA
