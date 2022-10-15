from setuptools import setup, Extension
import os

setup(name='pyco',
      ext_modules=[Extension('_pyco', ['pyco/_pyco.cpp', 'third_party/libaco/aco.c'], include_dirs=['third_party/libaco'], extra_objects=['third_party/libaco/acosw.S'], extra_compile_args=['-g'], extra_link_args=['-g'])],)
