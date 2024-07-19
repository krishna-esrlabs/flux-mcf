/**
 * Cuda implementation of a [box filter](https://en.wikipedia.org/wiki/Box_blur).
 * 
 * Copyright (c) 2024 Accenture
 */

#ifndef MCFCUDADEMO_BOX_FILTER_H
#define MCFCUDADEMO_BOX_FILTER_H

#include <memory>
#include "mcf_cuda_demo_value_types/McfCudaDemoValueTypes.h"
#include "mcf_cuda/CudaMemory.h"


namespace mcf_cuda_demo {

class BoxFilter
{
public:
    BoxFilter(const int cudaDeviceId);

    ~BoxFilter();

    /**
    *
    */
    values::mcf_cuda_demo_value_types::demo_types::DemoImageUint8 blurImage(
        const values::mcf_cuda_demo_value_types::demo_types::DemoImageUint8& image,
        const int kernelSize);

private:
    using DemoImageUint8 = values::mcf_cuda_demo_value_types::demo_types::DemoImageUint8;

    /**
     * The cuda device id.
     */
    int fCudaDeviceId;

    /**
     * Working memory for input image in GPU memory.
     */
    mcf::cuda::unique_array<uint8_t> fRawImage;

};

} // namespace mcf_cuda_demo


#endif /* MCFCUDADEMO_BOX_FILTER_H */
