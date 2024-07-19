/**
 * See header file for documentation.
 * 
 * Copyright (c) 2024 Accenture
 */
#include "mcf_cuda_demo/ImageFilterCudaComponent.h"
#include "mcf_core/util/JsonValueExtractor.h"
#include "mcf_core/LoggingMacros.h"
#include "mcf_core/ErrorMacros.h"

#include <memory>


namespace {

void validateKernelSize(const int kernelSize)
{
    MCF_ASSERT(kernelSize % 2 != 0, "Kernel size should be odd.");
}

} // namespace


namespace mcf_cuda_demo {

ImageFilterCudaComponent::ImageFilterCudaComponent()
: mcf::Component("ImageFilterCudaComponent")
, fInvertedImageInPort(*this, "in_inverted_image", fImageQueueSize)
, fImageFilterParamsInPort(*this, "in_filter_params")
, fBlurredImageOutPort(*this, "out_blurred_image") {}


ImageFilterCudaComponent::~ImageFilterCudaComponent() = default;


void ImageFilterCudaComponent::configure(mcf::IComponentConfig& config) {
    updateConfigFromFile();

    if (!fBoxFilter)
    {
        fBoxFilter = std::make_unique<BoxFilter>(fCudaDeviceId);
    }

    // Register queued receiver port with a handler
    config.registerPort(fInvertedImageInPort);
    fInvertedImageInPort.registerHandler([this]{ onNewImage(); });

    // Register receiver port without a handler
    config.registerPort(fImageFilterParamsInPort);

    // Register sender port
    config.registerPort(fBlurredImageOutPort);
}


void ImageFilterCudaComponent::updateConfigFromFile()
{
    try
    {
        const Json::Value& config = getConfig();
        auto componentConfig = config["ImageFilterCudaComponent"];

        mcf::util::json::JsonValueExtractor valueExtractor("Image filter component configuration");
        fKernelSize = valueExtractor.extractConfigUInt(componentConfig["BoxFilterKernelSize"], "BoxFilterKernelSize");
        validateKernelSize(fKernelSize);
        fCudaDeviceId = valueExtractor.extractConfigInt(componentConfig["CudaDeviceId"], "BoxFilterCudaDeviceId");
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


void ImageFilterCudaComponent::updateImageFilterParams()
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


void ImageFilterCudaComponent::onNewImage()
{
    if (!fBoxFilter)
    {
        MCF_THROW_RUNTIME(getName() + " box filter has not been initialised.");
    }
    
    // Check if the port queue currently has a value.
    while (fInvertedImageInPort.hasValue())
    {
        MCF_DEBUG("New image received.");

        updateImageFilterParams();

        // Get the oldest value in the port queue. This operation consumes the value.
        const std::shared_ptr<const DemoImageUint8> image = fInvertedImageInPort.getValue();
        DemoImageUint8 blurredImage = fBoxFilter->blurImage(*image, fKernelSize);

        // Write the output MCF value to the sender port.
        fBlurredImageOutPort.setValue(std::move(blurredImage));
    }
}


} // namespace mcf_cuda_demo
