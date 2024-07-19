""""
Copyright (c) 2024 Accenture
"""
import os
from setuptools import setup, find_packages

with open("GETTING_STARTED.md", "r") as fh:
    long_description = fh.read()

script_path = os.path.dirname(os.path.realpath(__file__))
requirements_path = f'{script_path}/requirements.txt'
with open(requirements_path) as f:
    dependencies = list(f.read().splitlines())

setup(
    name="mcf",
    author="ESR Labs GmbH",
    description="mcf = minimal component framework",
    long_description=long_description,
    long_description_content_type="text/markdown",
    packages=find_packages(),
    install_requires=dependencies
)
