/**
 * Receives images on a QueuedReceiverPort. Every time a new image is received, it convolves each 
 * pixel in the image sequentially with a [box filter](https://en.wikipedia.org/wiki/Box_blur) 
 * kernel. It then sends the blurred image on a SenderPort.
 * 
 * Copyright (c) 2024 Accenture
 */
#ifndef MCFCPUDEMO_IMAGEFILTERCOMPONENT_H__
#define MCFCPUDEMO_IMAGEFILTERCOMPONENT_H__


#include "mcf_core/Mcf.h"
#include "mcf_cpu_demo_value_types/McfCpuDemoValueTypes.h"


namespace mcf_cpu_demo {

class ImageFilterComponent : public mcf::Component
{
public:
    ImageFilterComponent();
    virtual ~ImageFilterComponent();

private:
    using DemoImageUint8 = values::mcf_cpu_demo_value_types::demo_types::DemoImageUint8;
    using DemoImageFilterParams = values::mcf_cpu_demo_value_types::demo_types::DemoImageFilterParams;

    /**
     * See base class
     */
    void configure(mcf::IComponentConfig& config) override;

    void updateConfigFromFile();

    /**
     * Blurs image and then publishes result on fBlurredImageOutPort. 
     * 
     * Function is registered as a port handler to fInvertedImageInPort in configure(). Handler will 
     * be called every time a new value is pushed to fInvertedImageInPort. 
     */
    void onNewImage();

    /**
     * Checks the current image filter parameters on fImageFilterParamsInPort.
     */
    void updateImageFilterParams();

    /**
     * Blurs image using a box filter with variable kernel size. Kernel size is set by the value on
     * fImageFilterParamsInPort.
     */
    DemoImageUint8 blurImage(const DemoImageUint8& image) const;

    /**
     * Queue size for incoming images.
     */
    const size_t fImgQueueSize = 1;

    /**
     * Kernel size used by box filter.
     */
    uint8_t fKernelSize = 3;
    
    /**
     * Maintains a queue of received DemoImageUint8 values. These can be written by other components'
     * sender ports registered on the same topic or by the RemoteControl.
     */
    mcf::QueuedReceiverPort<DemoImageUint8> fInvertedImageInPort;

    /**
     * Receives the most recent DemoImageFilterParams value. This value can be written by other
     * components' sender ports registered on the same topic or by the RemoteControl.
     */
    mcf::ReceiverPort<DemoImageFilterParams> fImageFilterParamsInPort;

    /**
     * Sender port which writes DemoImageUint8 values to the value store which can be read by other 
     * components' receiver ports or by the RemoteControl.
     */
    mcf::SenderPort<DemoImageUint8> fBlurredImageOutPort;
};

} // namespace mcf_cpu_demo


#endif // MCFCPUDEMO_IMAGEFILTERCOMPONENT_H__
