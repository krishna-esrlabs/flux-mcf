"""
Script for trimming all traces in a directory

Copyright (c) 2024 Accenture
"""
import argparse
import asyncio
import datetime
import pathlib
import logging


async def run(cmdline: str, work_directory: pathlib.Path):
    """
    Runs a process asynchronously
    """
    logging.getLogger(__name__).debug(f"Running `{cmdline}` in {work_directory}")
    # return
    proc = await asyncio.create_subprocess_shell(
        cmdline,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        cwd=str(work_directory),
    )
    stdout, stderr = await proc.communicate()

    if proc.returncode != 0:
        print(f'[{cmdline!r} exited with {proc.returncode}]')
        if stdout:
            print(f'[stdout]\n{stdout.decode()}')
        if stderr:
            print(f'[stderr]\n{stderr.decode()}')


async def trim_ctf(file: pathlib.Path, suffix: str, begin: float, end: float):
    cmdline = f"babeltrace2 {file.name} -o ctf -w {file.name}-{suffix} " \
              f"--begin={begin} --end={end}"
    await run(cmdline, file.parent)


async def trim_ftrace(file: pathlib.Path, suffix: str, begin: float, end: float):
    basename = file.name
    splits = basename.split(".")
    new_name = "{}-{}.{}".format(splits[0], suffix, ".".join(splits[1:]))
    cmdline = f"trace-cmd split -i {basename} -o {new_name} {begin} {end}"
    await run(cmdline, file.parent)


async def trim_traces_in_directory(
        directory: pathlib.Path, suffix: str, begin: float, end: float
):
    """
    Scans a directory and trims all occurring traces
    """
    for item in directory.iterdir():
        if item.is_dir():
            # CTF trace directory
            if not item.name.endswith(suffix):
                await trim_ctf(item, suffix, begin, end)
        elif item.name.endswith(".dat"):
            # Kernel trace
            if not item.name.split(".")[0].endswith(suffix):
                await trim_ftrace(item, suffix, begin, end)
        else:
            # no trace
            pass


if __name__ == "__main__":
    """
    For a directory, consider each entry
    - If its extension is *.dat: assume kernel trace, use trace-cmd split <begin> <end>
    - If it is a directory: Use babeltrace2
    """
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-b",
        "--begin",
        type=float,
        default=0.,
        help="Beginning of the time interval, in seconds",
    )
    parser.add_argument(
        "-e",
        "--end",
        default=datetime.datetime.now().timestamp(),
        type=float,
        help="End of the time interval, in seconds",
    )
    parser.add_argument(
        "-s", "--suffix", type=str, default="trim", help="Suffix for trimmed trace"
    )
    parser.add_argument(
        "directory", metavar="DIR", type=str, help="Directory where the traces reside"
    )
    args = parser.parse_args()

    asyncio.run(
        trim_traces_in_directory(
            pathlib.Path(args.directory), args.suffix, args.begin, args.end
        )
    )
