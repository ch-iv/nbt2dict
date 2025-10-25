from setuptools import setup
from setuptools import Extension


setup(
    name='nbt2dict-lib',
    version='1',
    description='Named Binary Tag to dictionary converter.',
    ext_modules=[
        Extension(
            "_nbt2dict",
            [
                "_nbt2dict.c",
            ],
        ),
    ],
)
