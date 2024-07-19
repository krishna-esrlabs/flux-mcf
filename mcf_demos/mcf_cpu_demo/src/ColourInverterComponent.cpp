/**
 * See header file for documentation.
 * 
 * Copyright (c) 2024 Accenture
 */
#include "mcf_cpu_demo/ColourInverterComponent.h"


namespace mcf_cpu_demo {

ColourInverterComponent::ColourInverterComponent()
: mcf::Component("ColourInverterComponent")
, fImageInPort(*this, "in_image", fImgQueueSize)
, fInvertedImageOutPort(*this, "out_inverted_image") {}


ColourInverterComponent::~ColourInverterComponent() = default;


void ColourInverterComponent::configure(mcf::IComponentConfig& config) {
    config.registerPort(fImageInPort);
    fImageInPort.registerHandler([this]{ onNewImage(); });

    config.registerPort(fInvertedImageOutPort);
}


void ColourInverterComponent::onNewImage()
{
    while (fImageInPort.hasValue())
    {
        const std::shared_ptr<const DemoImageUint8> image = fImageInPort.getValue();
        DemoImageUint8 invertedImage = invertImage(*image);
        fInvertedImageOutPort.setValue(std::move(invertedImage));
    }
}


values::mcf_cpu_demo_value_types::demo_types::DemoImageUint8 ColourInverterComponent::invertImage(const DemoImageUint8& image) const
{
    const uint8_t* imageBuffer = image.extMemPtr();
    const uint32_t imageBufferLength = image.extMemSize();

    std::unique_ptr<uint8_t[]> invertedImageBuffer = std::make_unique<uint8_t[]>(imageBufferLength);
    
    for (size_t i=0; i<imageBufferLength; ++i)
    {
        invertedImageBuffer[i] = 255 - imageBuffer[i];
    }

    DemoImageUint8 invertedImage(image.width, image.height, image.pitch, image.format, image.timestamp);
    invertedImage.extMemInit(std::move(invertedImageBuffer), imageBufferLength);
    
    return invertedImage;
}

} // namespace mcf_cpu_demo
