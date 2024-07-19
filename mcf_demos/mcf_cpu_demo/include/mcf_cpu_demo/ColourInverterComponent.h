/**
 * Receives images on a QueuedReceiverPort. Every time a new image is received, it inverts the image 
 * intensities by taking (255 - pixel_intensity) for each colour channel of each pixel in the image. 
 * It then sends the inverted image on a SenderPort.
 * 
 * Copyright (c) 2024 Accenture
 */
#ifndef MCFCPUDEMO_COLOURINVERTERCOMPONENT_H_
#define MCFCPUDEMO_COLOURINVERTERCOMPONENT_H_


#include "mcf_core/Mcf.h"
#include "mcf_cpu_demo_value_types/McfCpuDemoValueTypes.h"


namespace mcf_cpu_demo {

class ColourInverterComponent : public mcf::Component
{
public:
    ColourInverterComponent();
    virtual ~ColourInverterComponent();

private:
    using DemoImageUint8 = values::mcf_cpu_demo_value_types::demo_types::DemoImageUint8;

    void configure(mcf::IComponentConfig& config) override;
    
    void onNewImage();
    DemoImageUint8 invertImage(const DemoImageUint8& image) const;

    /**
     * Queue size for incoming images.
     */
    const size_t fImgQueueSize = 1;

    mcf::QueuedReceiverPort<DemoImageUint8> fImageInPort;
    mcf::SenderPort<DemoImageUint8> fInvertedImageOutPort;
};

} // namespace mcf_cpu_demo

#endif // MCFCPUDEMO_COLOURINVERTERCOMPONENT_H_
