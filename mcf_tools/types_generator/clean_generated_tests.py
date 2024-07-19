""""
Copyright (c) 2024 Accenture
"""
import argparse
import os
import shutil
from pathlib import Path


def fast_scandir(dirname: 'Path'):
    if os.path.exists(dirname):
        subfolders = [f.path for f in os.scandir(dirname) if f.is_dir()]

        for dirname in list(subfolders):
            subfolders.extend(fast_scandir(dirname))
        return subfolders
    else:
        return []


def remove_subfolders(path: 'Path'):
    subfolders = fast_scandir(path)
    for folder in subfolders:
        if os.path.exists(folder):
            shutil.rmtree(folder)


def clean_value_tests(output_directory: str):
    output_directory_path = Path(output_directory)

    # clean generated cpp test headers
    test_include_path = output_directory_path / 'include'
    remove_subfolders(test_include_path)

    # clean generated cpp test main
    test_main_path = output_directory_path / 'main_test'
    remove_subfolders(test_main_path)

    # clean generated cpp test src
    test_src_path = output_directory_path / 'src'
    if test_src_path.exists():
        shutil.rmtree(test_src_path)

    # clean generated python test
    test_python_path = output_directory_path / 'python'
    if test_python_path.exists():
        shutil.rmtree(test_python_path)


def main():
    parser = argparse.ArgumentParser(
        description='Clean generated python and c++ value type test files')
    parser.add_argument('-o', '--output', required=True,
                        help='Base output directory where generated test code were generated.'
                             ' Should be the same value used in tester_generator.py.')

    args = parser.parse_args()
    output = args.output
    clean_value_tests(output)


if __name__ == '__main__':
    main()
