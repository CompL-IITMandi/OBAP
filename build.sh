#!/bin/bash

if [ -z "$1" ]; then 
    echo "[ERROR] BUILD_DIR missing"
    echo "USAGE: ./build.sh BUILD_DIR LLVM_DIR GNUR_DIR";
    exit
fi

if [ -z "$2" ]; then 
    echo "[ERROR] LLVM_DIR missing"
    echo "USAGE: ./build.sh BUILD_DIR LLVM_DIR GNUR_DIR";
    exit
fi

if [ -z "$3" ]; then 
    echo "[ERROR] GNUR_DIR missing"
    echo "USAGE: ./build.sh BUILD_DIR LLVM_DIR GNUR_DIR";
    exit
fi


echo "=== BUILDING OBAP ==="
echo "Build DIR: $1"
echo "LLVM  DIR: $2"
echo "GNUR  DIR: $3"

cwd=`pwd`

cd $1
export LD_LIBRARY_PATH=$3/lib/
cmake -DLLVM_DIR=$2 -DR_BUILD=$3 ..
make

cd $cwd

echo "LD_LIBRARY_PATH=$3/lib/ R_HOME=$3 $1/bcp \$1" > run.sh
chmod +x run.sh
exit