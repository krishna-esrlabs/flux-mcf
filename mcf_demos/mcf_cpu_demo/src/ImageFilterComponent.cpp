/**
 * See header file for documentation.
 * 
 * Copyright (c) 2024 Accenture
 */
#include "mcf_cpu_demo/ImageFilterComponent.h"
#include "mcf_core/util/JsonValueExtractor.h"
#include "mcf_core/LoggingMacros.h"
#include "mcf_core/ErrorMacros.h"
#include <memory>


namespace {

size_t getBufferIdx(const size_t rowIdx, const size_t colIdx, const uint32_t pitch, uint16_t height)
{
    MCF_ASSERT(rowIdx >= 0 && rowIdx < height);
    MCF_ASSERT(colIdx >= 0 && colIdx < pitch);
    return rowIdx * pitch + colIdx;
}


void validateKernelSize(const int kernelSize)
{
    MCF_ASSERT(kernelSize % 2 != 0, "Kernel size should be odd.");
}

} // namespace


namespace mcf_cpu_demo {

ImageFilterComponent::ImageFilterComponent()
: mcf::Component("ImageFilterComponent")
, fInvertedImageInPort(*this, "in_inverted_image", fImgQueueSize)
, fImageFilterParamsInPort(*this, "in_filter_params")
, fBlurredImageOutPort(*this, "out_blurred_image") {}


ImageFilterComponent::~ImageFilterComponent() = default;


void ImageFilterComponent::configure(mcf::IComponentConfig& config) {
    updateConfigFromFile();

    // Register queued receiver port with a handler
    config.registerPort(fInvertedImageInPort);
    fInvertedImageInPort.registerHandler([this]{ onNewImage(); });

    // Register receiver port without a handler
    config.registerPort(fImageFilterParamsInPort);

    // Register sender port
    config.registerPort(fBlurredImageOutPort);
}


void ImageFilterComponent::updateConfigFromFile()
{
    try
    {
        const Json::Value& config = getConfig();
        auto componentConfig = config["ImageFilterComponent"];

        mcf::util::json::JsonValueExtractor valueExtractor("Image filter component configuration");
        fKernelSize = valueExtractor.extractConfigUInt(componentConfig["BoxFilterKernelSize"], "BoxFilterKernelSize");
        validateKernelSize(fKernelSize);
        MCF_DEBUG_NOFILELINE(getName() + " successfully parsed configuration file.");
    }
    catch (Json::RuntimeError& e)
    {
        MCF_ERROR(getName() + " failed to parse configuration file.");
        MCF_ERROR_NOFILELINE(e.what());
        MCF_THROW_ERROR(Json::RuntimeError, getName() + " failed to parse configuration file");
    }
    catch (std::runtime_error& e)
    {
        MCF_ERROR(getName() + " failed to obtain configuration.");
        MCF_ERROR_NOFILELINE(e.what());
        MCF_THROW_RUNTIME(getName() + " failed to obtain configuration");
    }
}


void ImageFilterComponent::updateImageFilterParams()
{
    // Check if the port currently has a value.
    if (fImageFilterParamsInPort.hasValue())
    {
        // Get the current value on the port. Since this is a receiverPort, it does not consume the value.
        const std::shared_ptr<const DemoImageFilterParams> params = fImageFilterParamsInPort.getValue();
        fKernelSize = params->kernelSize;
        validateKernelSize(fKernelSize);
    }
}


void ImageFilterComponent::onNewImage()
{
    // Check if the port queue currently has a value.
    while (fInvertedImageInPort.hasValue())
    {
        MCF_DEBUG("New image received.");

        updateImageFilterParams();

        // Get the oldest value in the port queue. This operation consumes the value.
        const std::shared_ptr<const DemoImageUint8> image = fInvertedImageInPort.getValue();
        DemoImageUint8 blurredImage = blurImage(*image);

        // Write the output MCF value to the sender port.
        fBlurredImageOutPort.setValue(std::move(blurredImage));
    }
}


values::mcf_cpu_demo_value_types::demo_types::DemoImageUint8 ImageFilterComponent::blurImage(const DemoImageUint8& image) const
{
    int numChannels = (image.format == values::mcf_cpu_demo_value_types::demo_types::GRAY) ? 1 : 3;

    // Output image will be cropped according to the size of the box filter kernel.
    const uint16_t outputWidth = image.width - fKernelSize + 1;
    const uint16_t outputHeight = image.height - fKernelSize + 1;
    const uint32_t outputPitch = outputWidth * numChannels;
    const uint32_t outputImageBufferLength = outputPitch * outputHeight;

    std::unique_ptr<uint8_t[]> blurredImageBuffer = std::make_unique<uint8_t[]>(outputImageBufferLength);

    const uint8_t* imageBuffer = image.extMemPtr();
    const int halfSize = fKernelSize / 2;
    for (size_t rowIdx=0; rowIdx<outputHeight; ++rowIdx)
    {
        for (size_t colIdx=0; colIdx<outputPitch; ++colIdx)
        {
            const size_t outputIdx = getBufferIdx(rowIdx, colIdx, outputPitch, outputHeight);

            const size_t inputImgRowIdx = rowIdx + halfSize;
            const size_t inputImgColIdx = colIdx + (halfSize * numChannels);

            // Get the average of the pixels in the kernel neighbourhood in the original image.
            int pixelSum = 0;
            for (int i=-halfSize; i<halfSize + 1; ++i)
            {
                for (int j=-halfSize; j<halfSize + 1; ++j)
                {
                    const size_t inputIdx = getBufferIdx(
                        inputImgRowIdx + j,
                        inputImgColIdx + (i * numChannels),
                        image.pitch,
                        image.height);
                    pixelSum += imageBuffer[inputIdx];
                }

            }
            blurredImageBuffer[outputIdx] = round(static_cast<float>(pixelSum) / (fKernelSize * fKernelSize));
        }
    }

    // Initialise the output MCF value.
    DemoImageUint8 blurredImage(outputWidth, outputHeight, outputPitch, image.format, image.timestamp);

    // Initialise ExtMemValue of the output MCF value with the blurred image buffer.
    blurredImage.extMemInit(std::move(blurredImageBuffer), outputImageBufferLength);

    return blurredImage;
}

} // namespace mcf_cpu_demo
