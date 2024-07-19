""""
Helper classes for sending and receiving images or generic data from Python to C++ using the Remote 
Control interface.

Copyright (c) 2024 Accenture
"""
import numpy as np
from typing import Optional, TYPE_CHECKING, Type

from value_types.mcf_cpu_demo_value_types.demo_types.DemoImageUint8 import DemoImageUint8
from value_types.mcf_cpu_demo_value_types.demo_types.DemoImgFormat import DemoImgFormat

if TYPE_CHECKING:
    import mcf_python_path.mcf_paths
    from mcf import RemoteControl, ValueT


class McfValueCommunicator:
    """
    Send and receive values to / from MCF via the remote control.

    :param remote_control:  the mcf remote control
    """

    def __init__(self, remote_control: 'RemoteControl'):
        self._remote_control = remote_control

    def send_value(self, value: 'ValueT', value_type: Type['ValueT'], topic: str) -> None:
        value_sender = self._remote_control.create_sender_accessor(
            value_type=value_type,
            topic=topic)

        try:
            value_sender.set(value)
        except Exception as e:
            print('Exception in send_value()')
            raise e

    def get_value(self, value_type: Type['ValueT'], topic: str) -> 'ValueT':
        value_receiver = self._remote_control.create_receiver_accessor(
            value_type=value_type,
            topic=topic)

        try:
            value = value_receiver.get()
        except Exception as e:
            print('Exception in get_value()')
            raise e
        return value


class McfImageSender(McfValueCommunicator):
    """
    Send an image to MCF via the remote control.
    """
    def send_img_uint8(self, image_data: np.ndarray, topic: str) -> None:
        """
        Send the given image to the given topic on the value store

        :param image_data:         image data, may be grayscale, RGB or None
        :param topic:              the value store topic
        """
        processed_img = self.process_raw_image(image_data)
        self.send_value(
            value=processed_img,
            value_type=DemoImageUint8,
            topic=topic)

    @staticmethod
    def process_raw_image(image: np.ndarray) -> DemoImageUint8:
        # If image is None, create dummy value with image format NONE (= 0);
        # otherwise create image value from given data
        if image is None:
            return DemoImageUint8()
        else:
            image = np.asarray(image)

            assert len(image.shape) >= 2, "Image must have at least 2 dimensions"
            assert len(image.shape) <= 3, "Image must not have more than 3 dimensions"

            # Determine the number of image channels and derive the image type
            #
            # Grayscale
            if len(image.shape) < 3 or image.shape[2] == 1:
                image = np.reshape(image, image.shape[:2])
                img_type = DemoImgFormat.GRAY
                pixel_size = 1

            # RGB
            elif image.shape[2] == 3:
                img_type = DemoImgFormat.RGB
                pixel_size = 3

            else:
                raise ValueError("Image data (dimensions {}) "
                                 "not grayscale or RGB".format(str(image.shape)))

            # prepare data for value store
            height, width = image.shape[:2]
            pitch = width * pixel_size
            img_data = np.reshape(image, [-1]).astype(np.uint8)
            timestamp = 0
            img_data = img_data.tobytes()
            value = DemoImageUint8(
                width,
                height,
                pitch,
                img_type,
                timestamp,
                bytearray(img_data))

            return value


class McfImageReceiver(McfValueCommunicator):
    """
    Receive an image from MCF via the remote control.
    """
    def get_img_uint8(self, topic: str) -> Optional[np.ndarray]:
        image = self.get_value(DemoImageUint8, topic)

        if image is None:
            return None
        else:
            return self.process_raw_image(image)

    @staticmethod
    def process_raw_image(image: 'ValueT') -> np.ndarray:
        width = image.width
        height = image.height
        pitch = image.pitch
        img_data = np.frombuffer(image.data, dtype=np.uint8).reshape([height, pitch])

        if image.format == DemoImgFormat.GRAY:
            img_data = img_data[:, 0:width]

        elif image.format == DemoImgFormat.RGB or image.format == DemoImgFormat.BGR:
            img_data = img_data[:, 0:3*width].reshape([height, width, 3])

        else:
            raise ValueError("Image type {} not supported".format(image.format))

        return img_data
