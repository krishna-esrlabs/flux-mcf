/**
 * See header file for documentation.
 *
 * Copyright (c) 2024 Accenture
 */
#include "mcf_value_type_demo/PoseReceiverComponent.h"
#include <iostream>

namespace {

void printPose(const values::mcf_example_types::odometry::Pose& poseMsg)
{
    std::cout << "Received pose message." << std::endl;
    const auto& position = poseMsg.position;
    std::cout << "\tPosition : (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;

    const auto& rotationMatrix = poseMsg.rotation.rotation;
    std::cout << "\tRotation: [";
    std::for_each(rotationMatrix.begin(),
                  rotationMatrix.end() - 1,
                  [](const float& elem){ std::cout << elem << ", "; });
    std::cout << rotationMatrix.back() << "]" << std::endl;
}

}  // namespace

namespace mcf_value_type_demo {

PoseReceiverComponent::PoseReceiverComponent()
: mcf::Component("PoseReceiverComponent")
, fPoseInPort(*this, "in_pose", fPoseQueueSize) {}


PoseReceiverComponent::~PoseReceiverComponent() = default;


void PoseReceiverComponent::configure(mcf::IComponentConfig& config)
{
    config.registerPort(fPoseInPort);
    fPoseInPort.registerHandler([this]{ onNewPose(); });
}


void PoseReceiverComponent::onNewPose()
{
    while (fPoseInPort.hasValue())
    {
        const std::shared_ptr<const Pose> pose = fPoseInPort.getValue();
        printPose(*pose);
    }
}

}  // namespace mcf_value_type_demo
