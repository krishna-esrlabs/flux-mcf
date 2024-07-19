""""
Copyright (c) 2024 Accenture
"""
import argparse
import os
import shutil
import typing
from pathlib import Path


def fast_scandir(dirname: 'Path') -> typing.List[str]:
    if os.path.exists(dirname):
        subfolders = [f.path for f in os.scandir(dirname) if f.is_dir()]

        for dirname in list(subfolders):
            subfolders.extend(fast_scandir(dirname))
        return subfolders
    else:
        return []


def remove_subfolders(path: 'Path'):
    removed_folder = False
    subfolders = fast_scandir(path)
    for folder in subfolders:
        if os.path.exists(folder):
            shutil.rmtree(folder)
            removed_folder = True
    return removed_folder


def clean_value_types(output_cpp_dir: 'Path', output_py_dir: 'Path', input_dir: 'Path'):
    # clean generated cpp headers
    if remove_subfolders(output_cpp_dir):
        print(f'Removed C++ generated value types:    {output_cpp_dir}')

    # clean generated python
    if remove_subfolders(output_py_dir):
        print(f'Removed python generated value types: {output_py_dir}')

    # clean value_types cache
    scan_file = input_dir / '.cache.pkl'
    if os.path.exists(scan_file):
        os.remove(scan_file)
        print(f'Removing generated value types cache: {scan_file}')


def main():
    parser = argparse.ArgumentParser(
        description='Clean generated python and c++ value type files')
    parser.add_argument('-o_cpp', '--output_cpp_dir', required=True,
                        help='Base output directory where cpp header files were generated. '
                             'Should be the same value used in value_type_generator.py')
    parser.add_argument('-o_py', '--output_py_dir', required=True,
                        help='Base output directory where python files were generated. '
                             'Should be the same value used in value_type_generator.py')
    parser.add_argument('-i', '--input_dir', required=True,
                        help='Input directory containing json description files for '
                             'classes that were generated. Should be the same value '
                             'used in value_type_generator.py')

    args = parser.parse_args()
    output_cpp_dir = Path(args.output_cpp_dir)
    output_py_dir = Path(args.output_py_dir)
    input_dir = Path(args.input_dir)
    clean_value_types(output_cpp_dir, output_py_dir, input_dir)


if __name__ == '__main__':
    main()
