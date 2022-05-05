cmake_minimum_required(VERSION 2.8.8)

project(bcp)

# set R -- This should be set manually
set(LLVM_DIR ../serial_upgrade/external/llvm-12)

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

file(GLOB_RECURSE SRC "main/opt/*.cpp" "main/rir/*.cpp" "main/utils/*.cpp")
include_directories(SYSTEM "./main")

set(CMAKE_CXX_FLAGS "${LLVM_CXX_FLAGS} ${CMAKE_CXX_FLAGS_LIBCXX} -Wall -Wuninitialized -Wundef -Winit-self -Wcast-align -Woverloaded-virtual -Wmissing-include-dirs -Wstrict-overflow=3 -std=c++14 -fno-rtti -fno-exceptions -Wimplicit-fallthrough -Wno-deprecated-declarations")

add_executable(bcp main.cpp ${SRC})

if (DEFINED LLVM_PACKAGE_VERSION)
  llvm_map_components_to_libnames(LLVM_LIBS ${LLVM_COMPONENTS_USED})
  target_link_libraries(${PROJECT_NAME} ${LLVM_LIBS})
endif(DEFINED LLVM_PACKAGE_VERSION)
