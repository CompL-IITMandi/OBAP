# Offline bitcode analysis and processing for Rsh serialized LLVM bitcodes

## Setting LLVM path
Update the LLVM_DIR in the CMakeLists.txt to point to the llvm version you want to use.

### Installation
```
mkdir build
cd build
cmake ..
make
```

### Usage
The first argument to bcp is the path to the processed json file.
The second argument is the path that contains the folders that are to be processed. The folders that are to be analyzed are indicated by the the json so only the outer directory containing the folders is required.

```
cd build
./bcp ../tests/processedBitcode.json ../tests
```
