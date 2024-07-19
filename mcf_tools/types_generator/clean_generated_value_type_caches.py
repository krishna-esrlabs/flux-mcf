""""
Copyright (c) 2024 Accenture
"""
import argparse
import os
from pathlib import Path


def clean_caches(parent_dir: Path, cache_filename: str):
    for path in parent_dir.rglob(cache_filename):
        os.remove(path)
        print(f'Removed cache file: {path}')


def main():
    parser = argparse.ArgumentParser(
        description='Recursively search for and remove all generated value type cache files below '
                    'the specified parent directory. This will cause the value type generator to '
                    're-generate these value types.')
    parser.add_argument('-d', '--parent_dir', required=True,
                        help='Parent directory which will be recursively searched for all value '
                             'type cache files.')
    parser.add_argument('-n', '--cache_filename', required=False, default=".cache.pkl",
                        help='Name of cache files to search for.')

    args = parser.parse_args()
    parent_dir = Path(args.parent_dir)
    cache_filename = args.cache_filename
    clean_caches(parent_dir, cache_filename)


if __name__ == '__main__':
    main()
