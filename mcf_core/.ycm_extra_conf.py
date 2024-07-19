import os

ws = os.path.abspath(os.path.dirname(__file__))

def FlagsForFile( filename, **kwargs ):
  return {
    'flags': [ '-x', 'c++', '-Wall', '-Wextra', '-Werror',
        '-I', ws + '/include',
        '-I', ws + '/vm_includes',
        '-I', ws + '/../gtest/fused-src',
        '-I', ws + '/../msgpack-c/include'
    ],
  }

