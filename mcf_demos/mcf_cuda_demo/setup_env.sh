#!/bin/bash
SCRIPT_DIR=$(dirname -- $0)

# Add the generated python value types to the python path
export PYTHONPATH=${SCRIPT_DIR}/mcf_cuda_demo_value_types/python:../../mcf_tools/:${PYTHONPATH}
