# Offline bitcode analysis and processing for Rsh serialized LLVM bitcodes

### Prerequisites

#### 1. LLVM

LLVM binaries can be downloaded directly from the [LLVM releases](https://github.com/llvm/llvm-project/releases).

It is recommended that the same version that was used to serialize the bitcodes is used to process them aswell. If rsh is already setup then you can simply point the -DLLVM_DIR in cmake to the external/llvm-12 folder as follows.

```console
cmake -DLLVM_DIR=/PATH_TO_RSH/external/llvm-12 -DR_BUILD=/PATH/GNUR ..
```

#### 2. Building GNUR

Download the GNUR from [CRAN](https://cran.r-project.org/).

This project was built and tested using GNUR 4.4.1 and can be found at [GNUR 4.1.1](https://cran.r-project.org/src/base/R-4/R-4.1.1.tar.gz).
```console
# Extract the R-4.1.1.tar.gz into a folder (lets say /PATH/GNUR)
cd /PATH/GNUR
# Configure the project (--with-x --with-readline are optional)
./configure --with-x --with-readline --enable-R-shlib
# Make the project
make
# After building the project it should give you these three files
ls lib/*.so
lib/libRblas.so  lib/libRlapack.so  lib/libR.so
```

### Installation
```
mkdir build
cd build
cmake -DLLVM_DIR=/PATH/LLVM -DR_BUILD=/PATH/GNUR ..
make
```

### Usage
It requires the R_HOME environment variable to be set manually to the path where the GNUR shared library was built (see the Prerequisites section)
The serializer creates .bc and .meta files, the path containing these files must be passed to the program.
```
cd build
R_HOME=/PATH/GNUR ./bcp /PATH/SERIALIZED_BITCODES_FOLDER
```
