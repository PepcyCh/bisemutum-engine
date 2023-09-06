cmake_minimum_required(VERSION 3.22)

project(pep-cprep LANGUAGES C CXX VERSION 0.1.0)


include(CMakeDependentOption)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(CPREP_MASTER_PROJECT ON)
else()
    set(CPREP_MASTER_PROJECT OFF)
endif()

option(CPREP_BUILD_OBJECT_LIB "build pep-cprep as object library" OFF)
cmake_dependent_option(CPREP_BUILD_TESTS "build tests of pep-cprep" ON "CPREP_MASTER_PROJECT" OFF)
cmake_dependent_option(CPREP_BUILD_BIN "build binary of pep-cprep" ON "CPREP_MASTER_PROJECT" OFF)


file(GLOB_RECURSE CPREP_PUBLIC_SOURCES include/*.hpp)
file(GLOB_RECURSE CPREP_PRIVATE_SOURCES src/*.cpp src/*.hpp)
if(NOT CPREP_BUILD_OBJECT_LIB)
    add_library(pep-cprep ${CPREP_PUBLIC_SOURCES} ${CPREP_PRIVATE_SOURCES})
else()
    add_library(pep-cprep OBJECT ${CPREP_PUBLIC_SOURCES} ${CPREP_PRIVATE_SOURCES})
endif()
target_compile_features(pep-cprep PUBLIC cxx_std_20)
target_include_directories(pep-cprep PUBLIC include)

if(CPREP_INLINE_NAMESPACE AND NOT CPREP_INLINE_NAMESPACE STREQUAL "")
    target_compile_definitions(pep-cprep PUBLIC PEP_CPREP_INLINE_NAMESPACE=${CPREP_INLINE_NAMESPACE})
endif()


if(CPREP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()


if(CPREP_BUILD_BIN)
    add_executable(pep-cprep-bin bin/main.cpp)
    target_link_libraries(pep-cprep-bin PRIVATE pep-cprep)
endif()
