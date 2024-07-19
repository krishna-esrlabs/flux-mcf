/**
 * Receives poses on a QueuedReceiverPort. Every time a new pose is received, it is
 * printed to the screen.
 *
 * Copyright (c) 2024 Accenture
 */
#ifndef MCFVALUETYPEDEMO_POSERECEIVERCOMPONENT_H_
#define MCFVALUETYPEDEMO_POSERECEIVERCOMPONENT_H_


#include "mcf_core/Mcf.h"
#include "mcf_example_types/McfExampleTypes.h"


namespace mcf_value_type_demo {

class PoseReceiverComponent : public mcf::Component
{
public:
    PoseReceiverComponent();
    virtual ~PoseReceiverComponent();

private:
    using Pose = values::mcf_example_types::odometry::Pose;

    void configure(mcf::IComponentConfig& config) override;
    
    void onNewPose();

    /**
     * Queue size for incoming poses.
     */
    const size_t fPoseQueueSize = 1;

    mcf::QueuedReceiverPort<Pose> fPoseInPort;
};

} // namespace mcf_cpu_demo

#endif // MCFVALUETYPEDEMO_POSERECEIVERCOMPONENT_H_
