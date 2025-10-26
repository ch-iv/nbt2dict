from setuptools import setup, Extension

nbt2dict_extension = Extension("nbt2dict", ["nbt2dict.c"])

setup(ext_modules=[nbt2dict_extension])
