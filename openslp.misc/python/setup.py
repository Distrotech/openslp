import sys

from distutils.core import setup
from distutils.extension import Extension
from Pyrex.Distutils import build_ext

setup(
  name = "slp",
  ext_modules=[ 
    Extension("slp", ["slp.pyx"], libraries = ["slp"])
    ],
  cmdclass = {'build_ext': build_ext}
)

