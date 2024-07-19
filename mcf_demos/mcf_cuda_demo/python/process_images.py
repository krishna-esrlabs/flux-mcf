""""
Establishes the python-side connection of the MCF Remote Control. It uses the Python Remote Control 
interface to send parameters and an image to the C++ Remote Control. It also receives images on
various topics from the image processing pipeline and displays them on the screen.

Copyright (c) 2024 Accenture
"""
import os
import argparse
from PIL import Image
import numpy as np
import typing

import mcf_python_path.mcf_paths
from mcf import RemoteControl
from mcf_sender_receiver import McfValueCommunicator, McfImageSender, McfImageReceiver

from value_types.mcf_cuda_demo_value_types.demo_types.DemoImageFilterParams import DemoImageFilterParams


class McfDemo:
    raw_img_topic = "/image/raw"
    inverted_img_topic = "/image/inverted"
    filter_params_topic = "/filter/params"
    blurred_img_topic = "/image/blurred"
    
    def __init__(self, target_ip: str, target_port: int):
        self.remote_control = RemoteControl()
        self.remote_control.connect(target_ip, target_port)

        self.mcf_image_sender = McfImageSender(self.remote_control)
        self.mcf_image_receiver = McfImageReceiver(self.remote_control)
        self.mcf_value_communicator = McfValueCommunicator(self.remote_control)

        self._initialise_port_queues()

    def _initialise_port_queues(self):
        # Set queue size
        self.remote_control.set_queue(McfDemo.raw_img_topic, 1, blocking=True)
        self.remote_control.set_queue(McfDemo.inverted_img_topic, 1, blocking=True)
        self.remote_control.set_queue(McfDemo.blurred_img_topic, 1, blocking=True)

    def send_image_filter_params(self, kernel_size: int):
        demo_image_filter_params = DemoImageFilterParams(kernel_size)
        self.mcf_value_communicator.send_value(
            value=demo_image_filter_params,
            value_type=DemoImageFilterParams,
            topic=McfDemo.filter_params_topic)

    def send_raw_image(self, image: np.ndarray):
        self.mcf_image_sender.send_img_uint8(image, McfDemo.raw_img_topic)

    def wait_for_received_images(self) -> typing.Tuple[np.ndarray, np.ndarray, np.ndarray]:
        raw_img = None
        inverted_img = None
        blurred_img = None
        while raw_img is None or inverted_img is None or blurred_img is None:
            if raw_img is None:
                raw_img = self.mcf_image_receiver.get_img_uint8(McfDemo.raw_img_topic)

            if inverted_img is None:
                inverted_img = self.mcf_image_receiver.get_img_uint8(McfDemo.inverted_img_topic)

            if blurred_img is None:
                blurred_img = self.mcf_image_receiver.get_img_uint8(McfDemo.blurred_img_topic)

        return raw_img, inverted_img, blurred_img


def main(target_ip: str, target_port: int, input_img_path: str, kernel_size: int):
    assert os.path.exists(input_img_path), "Input image does not exist."
    input_img = Image.open(input_img_path)

    mcf_demo = McfDemo(target_ip, target_port)
    mcf_demo.send_image_filter_params(kernel_size)
    mcf_demo.send_raw_image(input_img)
    raw_img, inverted_img, blurred_img = mcf_demo.wait_for_received_images()

    raw_img_pil = Image.fromarray(raw_img)
    inverted_img_pil = Image.fromarray(inverted_img)
    blurred_img_pil = Image.fromarray(blurred_img)

    raw_img_pil.show("Raw")
    inverted_img_pil.show("Inverted")
    blurred_img_pil.show("Blurred")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('target_ip')
    parser.add_argument('target_port')
    parser.add_argument('input_img_path')
    parser.add_argument(
        "--kernel_size",
        default=3,
        type=int,
        help="Size of box filter kernel used by ImageFilterComponent")
    args = parser.parse_args()

    target_ip = args.target_ip
    target_port = args.target_port
    input_img_path = args.input_img_path
    kernel_size = args.kernel_size
    main(target_ip, target_port, input_img_path, kernel_size)
