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
```
source /cvmfs/sft.cern.ch/lcg/views/LCG_95/x86_64-centos7-gcc8-dbg/setup.sh 
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/lib64/pkgconfig
mkdir build && cd build
cmake ..
make
```

## Utilities:

src/utilities/add_files_to_gitconddb.py
build/read_gitconddb

# Examples:
```
python src/utilities/add_files_to_gitconddb.py --debug --since $(date +%s000000000 -d 2019-03-02) ~/supernemo_test_cdb/ ~/supernemo_test_cdb.git/
./build/read_gitconddb -r file:/home/user/supernemo_test_cdb.git -s detector2 -c condition2 -t $(date +%s000000000 -d 2019-03-02)
./read_gitconddb -v v1.2.0 -r git:/home/user/supernemo_test_cdb.git -s detector2 -c condition1 -t $(date +%s000000000 -d 2019-06-02)
```
