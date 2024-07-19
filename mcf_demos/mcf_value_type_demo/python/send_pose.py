""""
Establishes the python-side connection of the MCF Remote Control. It uses the Python Remote Control
interface to send parameters and an image to the C++ Remote Control. It also receives images on
various topics from the image processing pipeline and displays them on the screen.

Copyright (c) 2024 Accenture
"""
import mcf_python_path.mcf_paths
from mcf import RemoteControl

from value_types.mcf_example_types.odometry.Pose import Pose
from value_types.mcf_example_types.odometry.Position import Position
from value_types.mcf_example_types.odometry.Rotation import Rotation


ip_address = 'localhost'
port = '6666'


def main():
    # Create instance of RemoteControl and connect it to the main C++ process.
    remote_control = RemoteControl()
    remote_control.connect(ip_address, port)

    position_x = 0.0
    position_y = 5.0
    position_z = 10.0

    # 90 degree rotation about z
    rotation_matrix = [0.0, -1.0, 0.0,
                       1.0, 0.0, 0.0,
                       0.0, 0.0, 1.0]

    position_msg = Position(position_x, position_y, position_z)
    rotation_msg = Rotation(rotation_matrix)
    pose_msg = Pose(position_msg, rotation_msg)

    # ValueStore topic on which the pose_msg should be written
    topic = "/pose/input"

    # Serialize the pose msg
    value, value_type_name = pose_msg.serialize()

    # Send the pose value via the RemoteControl to the main process.
    remote_control.write_value(topic, value_type_name, value)


if __name__ == '__main__':
    main()
