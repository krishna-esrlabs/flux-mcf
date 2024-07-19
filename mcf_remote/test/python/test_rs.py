""""
Copyright (c) 2024 Accenture
"""
import inspect
import os
from pathlib import Path
import subprocess
import sys
import time


# path of mcf python tools, relative to location of this script
_MCF_TOOLS_RELATIVE_PATH = Path("../../../mcf_py")

# directory of this script and relative path of mcf python tools
_SCRIPT_DIRECTORY = Path(os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe()))))

# paths to executables to run
_CMAKE_GENERATOR_PATH = (_SCRIPT_DIRECTORY / "../../build/Generator/generator").resolve()
_CMAKE_RESPONDER_PATH = (_SCRIPT_DIRECTORY / "../../build/Responder/responder").resolve()

_BAKE_GENERATOR_PATH = (_SCRIPT_DIRECTORY / "../../../build/mcf_remote/test/generator").resolve()
_BAKE_RESPONDER_PATH = (_SCRIPT_DIRECTORY / "../../../build/mcf_remote/test/responder").resolve()


sys.path.append(os.path.join(_SCRIPT_DIRECTORY, _MCF_TOOLS_RELATIVE_PATH))
from mcf import RecordReader

# absolute path of executable to run on target
if _CMAKE_GENERATOR_PATH.is_file() and _CMAKE_RESPONDER_PATH.is_file():
    _GENERATOR_ABS_PATH = _CMAKE_GENERATOR_PATH
    _RESPONDER_ABS_PATH = _CMAKE_RESPONDER_PATH
elif _BAKE_GENERATOR_PATH.is_file() and _BAKE_RESPONDER_PATH.is_file():
    _GENERATOR_ABS_PATH = _BAKE_GENERATOR_PATH
    _RESPONDER_ABS_PATH = _BAKE_RESPONDER_PATH
else:
    raise FileNotFoundError("Could not find executables.")


def start_process(command, stdout=None):
    sp = subprocess.Popen(command, stdout=stdout)

    if sp is None:
        print("ERROR while starting generator")
        exit -1

    return sp


def filter_fun(record, port):
    return record.topic == port


def check_log(filename):
    rr = RecordReader()
    rr.open(filename)

    # check if the single const point has been logged
    rr.index(lambda record: filter_fun(record, "/generator/constPoint"))
    assert(rr.index_size() > 0)

    # check if some images have been logged
    rr.index(lambda record: filter_fun(record, "/generator/image"))
    numImages = rr.index_size()
    assert(numImages > 1)

    # check if the single const point has been logged
    rr.index(lambda record: filter_fun(record, "/generator/point"))
    numPoints = rr.index_size()
    assert(numPoints > 1)

    # check if some responses have been logged
    rr.index(lambda record: filter_fun(record, "/responder/response"))
    numResponses = rr.index_size()
    assert(numResponses > 1)

    return numImages, numPoints, numResponses


def test_mcf_rs():
    nValues = 500

    generator = start_process([str(_GENERATOR_ABS_PATH), "--n", str(nValues)])

    responder = start_process(str(_RESPONDER_ABS_PATH))

    # kill responder after one second
    time.sleep(1)
    responder.terminate()
    responder.communicate()

    responder.poll() # read return code
    assert(responder.returncode == 0)

    # parse the responder's log
    rni0, rnp0, _ = check_log("responderTrace.bin")

    # start new responder, should connect with generator automatically
    responder = start_process(str(_RESPONDER_ABS_PATH))

    # wait until the generator is done sending or kill after timeout
    timeout = 0
    while generator.poll() is None and timeout < 300:
        time.sleep(0.1)
        timeout += 1

    generator.terminate()
    generator.communicate()

    assert(generator.returncode == 0)

    gni, gnp, gnr = check_log("generatorTrace.bin")

    responder.terminate()
    responder.communicate()
    responder.poll()

    assert(responder.returncode == 0)

    rni1, rnp1, _ = check_log("responderTrace.bin")

    # check if all values have been created
    assert(gni + gnp == nValues)

    # at least half of the generated images/points should reach the receiver
    assert(gni/2 < (rni0 + rni1))
    assert(gnp/2 < (rnp0 + rnp1))

    # the responder should have received at least as many values as the generator got replies
    assert(rni0 + rni1 + rnp0 + rnp1 >= gnr-1)

    ################################################################################################

    responder = start_process([str(_RESPONDER_ABS_PATH),
                               "--connectionSend", "shm://1",
                               "--connectionRec", "shm://0"])

    generator = start_process([str(_GENERATOR_ABS_PATH), "--n", str(nValues),
                               "--connectionSend", "shm://0",
                               "--connectionRec", "shm://1"])

    # kill generator after five seconds
    time.sleep(5)
    generator.terminate()
    generator.communicate()

    generator.poll() # read return code
    assert(generator.returncode == 0)

    gni0, gnp0, _ = check_log("generatorTrace.bin")

    time.sleep(1)
    # start new generator
    generator = start_process([str(_GENERATOR_ABS_PATH), "--n", str(nValues),
                               "--connectionSend", "shm://0",
                               "--connectionRec", "shm://1"])

    # wait until the generator is done sending or kill after timeout
    timeout = 0
    while generator.poll() is None and timeout < 300:
        time.sleep(0.1)
        timeout += 1

    generator.terminate()
    generator.communicate()
    assert(generator.returncode == 0)

    gni1, gnp1, _ = check_log("generatorTrace.bin")

    responder.terminate()
    responder.communicate()
    responder.poll()
    assert(responder.returncode == 0)

    rni, rnp, rnr = check_log("responderTrace.bin")

    assert(rnr > nValues)  # more responses than the values in a single generator run should be sent

    # at least half of the generated images/points should reach the receiver
    assert(rni / 2 < (gni0 + gni1))
    assert(rnp / 2 < (gnp0 + gnp1))


if __name__ == "__main__":
    test_mcf_rs()
