/**
 * See header file for documentation.
 *
 * Copyright (c) 2024 Accenture
 */
#include "mcf_cuda_demo/BoxFilter.h"
#include "mcf_cuda/CudaErrorHelper.h"

#include <stdio.h>


namespace {

struct ImageSize
{
    ImageSize(uint16_t widthIn, uint16_t heightIn, uint8_t numChannelsIn)
    : width(widthIn)
    , height(heightIn)
    , numChannels(numChannelsIn)
    , size(widthIn * numChannelsIn * heightIn) 
    , pitch(widthIn * numChannelsIn) {};

    const uint16_t width;
    const uint16_t height;
    const uint8_t numChannels;
    const uint32_t size;
    const uint32_t pitch;
};


__global__ void blurRegion(
    const uint8_t* const rawImgBuffer,
    uint8_t* const filteredImgBuffer,
    const ImageSize inputImageSize,
    const ImageSize outputImageSize,
    const int kernelSize)
{
    const int outputIdx = blockIdx.x * blockDim.x + threadIdx.x;
    if (outputIdx >= outputImageSize.size)
    {
        return;
    }

    const uint32_t rowIdx = outputIdx / outputImageSize.pitch;
    const uint32_t colIdx = outputIdx % outputImageSize.pitch;

    const int halfSize = kernelSize / 2;

    const size_t inputImgRowIdx = rowIdx + halfSize;
    const size_t inputImgColIdx = colIdx + (halfSize * inputImageSize.numChannels);

    // Get the average of the pixels in the kernel neighbourhood in the original image.
    int pixelSum = 0;
    for (int i=-halfSize; i<halfSize + 1; ++i)
    {
        for (int j=-halfSize; j<halfSize + 1; ++j)
        {
            const int currentRowIdx = inputImgRowIdx + j;
            const int currentColIdx = inputImgColIdx + (i * outputImageSize.numChannels);
            const int inputIdx = currentRowIdx * inputImageSize.pitch + currentColIdx;
            pixelSum += rawImgBuffer[inputIdx];
        }
    }
    filteredImgBuffer[outputIdx] = __float2uint_rn((float) pixelSum / (kernelSize * kernelSize));
}

} // namespace


namespace mcf_cuda_demo {

BoxFilter::BoxFilter(const int cudaDeviceId) : fCudaDeviceId(cudaDeviceId) {}


BoxFilter::~BoxFilter() = default;


values::mcf_cuda_demo_value_types::demo_types::DemoImageUint8 BoxFilter::blurImage(
    const DemoImageUint8& image,
    const int kernelSize)
{
    MCF_CHECK_CUDA(cudaSetDevice(fCudaDeviceId));
    cudaDeviceProp deviceProp;
    MCF_CHECK_CUDA(cudaGetDeviceProperties(&deviceProp, fCudaDeviceId));
    MCF_CHECK_CUDA_ERROR;

    const int numChannels = (image.format == values::mcf_cuda_demo_value_types::demo_types::DemoImgFormat::GRAY) ? 1 : 3;
    
    const ImageSize inputImageSize(image.width, image.height, numChannels);
    fRawImage.increase(inputImageSize.size);

    const ImageSize outputImageSize(
        inputImageSize.width - kernelSize + 1,
        inputImageSize.height - kernelSize + 1,
        numChannels);

    // Copy image into pre-allocated GPU memory
    MCF_CHECK_CUDA(cudaMemcpy(
        fRawImage.get(), 
        image.extMemPtr(), 
        inputImageSize.size, 
        cudaMemcpyHostToDevice));
    MCF_CHECK_CUDA_ERROR;

    int blockSize = deviceProp.maxThreadsPerBlock;
    int numBlocks = (outputImageSize.size + blockSize - 1) / blockSize;

    // Allocate memory for output image in GPU memory
    mcf::cuda::unique_array<uint8_t> cudaArray = mcf::cuda::make_array<uint8_t>(outputImageSize.size);

    // Launch cuda kernel to blur image
    blurRegion<<<numBlocks, blockSize>>>(
        fRawImage.get(),
        cudaArray.get(),
        inputImageSize,
        outputImageSize,
        kernelSize
    );
    MCF_CHECK_CUDA_ERROR;

    // Initialise the output MCF value.
    DemoImageUint8 blurredImage(
        outputImageSize.width, 
        outputImageSize.height, 
        outputImageSize.pitch, 
        image.format, 
        image.timestamp);

    // Initialise ExtMemValue of the output MCF value with the blurred image buffer in GPU memory.
    blurredImage.extMemInit(std::move(cudaArray));
    MCF_CHECK_CUDA_ERROR;

    return blurredImage;
}

} // namespace mcf_cuda_demo
