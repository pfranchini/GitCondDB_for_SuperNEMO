# Git CondDB for SuperNEMO

This is a stand alone, experiment independent Conditions Database library using a Git repository as
storage backend, based on the code developed for the LHCb experiment.

Forked from https://gitlab.cern.ch/lhcb/GitCondDB/

## External software:

Software projects:
- [CMake](https://cmake.org) for building
- [Python](https://python.org) for helper scripts
- [Google Test](https://github.com/google/googletest) for unit testing
- [Clang Format](https://clang.llvm.org/docs/ClangFormat.html) for C++ code formatting
- [YAPF](https://github.com/google/yapf) for Python code formatting
- [LCOV](https://github.com/linux-test-project/lcov) for test coverage reports

Libraries:
- [libgit2](https://libgit2.org/) for the Git backend
- [JSON for Modern C++](https://nlohmann.github.io/json) for the JSON backend

## How to build:

source /cvmfs/sft.cern.ch/lcg/views/LCG_95/x86_64-centos7-gcc8-dbg/setup.sh 
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/lib64/pkgconfig
mkdir build && cd build
cmake ..
make

