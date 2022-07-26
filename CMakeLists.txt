cmake_minimum_required(VERSION 2.8.8)

project(bcp)

# 
# Include GNUR 4.1.1
#   This project needs GNUR built with R built using the flag ./configure --enable-R-shlib, read instructions for more
# 

option(R_BUILD "Path to GNUR build" NO)
if(R_BUILD)
  message("Using GNUR from: ${R_BUILD}")
else()
  message(FATAL_ERROR "R_BUILD must be specified (This project needs GNUR built with R built using the flag ./configure --enable-R-shlib, read instructions for more), cmake will exit now!")
endif()

if(NOT EXISTS "${R_BUILD}/lib/libR.so")
  message(FATAL_ERROR "libR.so not found in directory: ${R_BUILD}/lib")
endif()

if(NOT EXISTS "${R_BUILD}/lib/libRblas.so")
  message(FATAL_ERROR "libRblas.so not found in directory: ${R_BUILD}/lib")
endif()

if(NOT EXISTS "${R_BUILD}/lib/libRlapack.so")
  message(FATAL_ERROR "libRlapack.so not found in directory: ${R_BUILD}/lib")
endif()


# 
# Include LLVM: 
#   -DLLVM_DIR=/PATH_TO_LLVM/llvm-12
# Alternatively, simply using the binary downloaded with RSH should also work:
#   -DLLVM_DIR=/PATH_TO_RSH/external/llvm-12
#

option(LLVM_DIR "Path to llvm" NO)
if(LLVM_DIR)
  message("Using LLVM from: ${LLVM_DIR}")
else()
  message(FATAL_ERROR "LLVM_DIR must be specified (you can download the binary from https://github.com/llvm/llvm-project/releases), cmake will exit now!")
endif()

set(CMAKE_INSTALL_RPATH "${R_BUILD}/lib")
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Initialize LLVM includes and defs
include(${LLVM_DIR}/lib/cmake/llvm/LLVMConfig.cmake OPTIONAL)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
set(LLVM_COMPONENTS_USED core orcjit native vectorize lto bitwriter)
option(LLVM_USE_PERF "Include LLVM support for perf profiling." FALSE)
if (${LLVM_USE_PERF})
    set(LLVM_COMPONENTS_USED "${LLVM_COMPONENTS_USED}" PerfJITEvents)
    add_definitions(-DPIR_USE_PERF)
endif ()
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

#  Initialize GNUR includes
include_directories(SYSTEM "${R_BUILD}/include")

# Set flags
set(CMAKE_CXX_FLAGS "${LLVM_CXX_FLAGS} ${CMAKE_CXX_FLAGS_LIBCXX} -Wall -Wuninitialized -Wundef -Winit-self -Wcast-align -Woverloaded-virtual -Wmissing-include-dirs -Wstrict-overflow=3 -std=c++14 -fno-rtti -fno-exceptions -Wimplicit-fallthrough -Wno-deprecated-declarations -L/home/meetesh06/compL/BUILDS/CompL/R-4.1.1/lib")

set(CMAKE_EXE_LINKER_FLAGS 
          "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath -Wl,${R_BUILD}/lib")


# Include all headers from main
include_directories(SYSTEM "./main")

# Add all cpp sources
file(GLOB_RECURSE SRC "main/runtime/*.cpp" "main/opt/*.cpp" "main/rir/*.cpp" "main/utils/*.cpp")

add_executable(${PROJECT_NAME} main.cpp ${SRC})

find_library(LIB_R NAMES "libR.so" PATHS ${R_BUILD}/lib NO_DEFAULT_PATH)
find_library(LIB_RBLAS NAMES "libRblas.so" PATHS ${R_BUILD}/lib NO_DEFAULT_PATH)
find_library(LIB_RLAPACK NAMES "libRlapack.so" PATHS ${R_BUILD}/lib NO_DEFAULT_PATH)

message("Using libR.so from: ${LIB_R}")
message("Using libRblas.so from: ${LIB_RBLAS}")
message("Using libRlapack.so from: ${LIB_RLAPACK}")

target_link_libraries(${PROJECT_NAME} ${LIB_R})
target_link_libraries(${PROJECT_NAME} ${LIB_RBLAS})
target_link_libraries(${PROJECT_NAME} ${LIB_RLAPACK})

# Link LLVM libraries
if (DEFINED LLVM_PACKAGE_VERSION)
  llvm_map_components_to_libnames(LLVM_LIBS ${LLVM_COMPONENTS_USED})
  target_link_libraries(${PROJECT_NAME} ${LLVM_LIBS})
endif(DEFINED LLVM_PACKAGE_VERSION)
