from distutils.core import setup, Extension
setup(
    name="ponyguruma",
    packages=['ponyguruma'],
    ext_modules=[
        Extension("ponyguruma._lowlevel", ['ponyguruma/_lowlevel.c'],
                  library_dirs=['/usr/local/lib'],
                  libraries=['onig'])])
