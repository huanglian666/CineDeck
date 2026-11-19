cmake_minimum_required(VERSION 3.20)
project(ed2k_calc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(ed2k_calc STATIC
    src/ed2k_calc.c
)

target_include_directories(ed2k_calc
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 兼容 vcpkg 依赖：优先通过 CMAKE_TOOLCHAIN_FILE / CMAKE_PREFIX_PATH 定位 rhash，
# 并回退到仓库根目录 vcpkg_installed/<triplet>
# （传 toolchain 时跟随实际 triplet；本地未传 toolchain 时为 arm64-osx）
set(_VCPKG_FALLBACK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed")
if(VCPKG_TARGET_TRIPLET)
    set(_DEFAULT_VCPKG_PREFIX "${_VCPKG_FALLBACK_ROOT}/${VCPKG_TARGET_TRIPLET}")
else()
    set(_DEFAULT_VCPKG_PREFIX "${_VCPKG_FALLBACK_ROOT}/arm64-osx")
endif()

# rhash 头文件目录（供 #include <rhash.h> 使用）
find_path(RHASH_INCLUDE_DIR
    NAMES rhash.h
    HINTS "${_DEFAULT_VCPKG_PREFIX}/include"
)

# rhash 静态库
find_library(RHASH_LIBRARY
    NAMES rhash
    HINTS "${_DEFAULT_VCPKG_PREFIX}/lib" "${CMAKE_PREFIX_PATH}/lib"
)

if(RHASH_INCLUDE_DIR)
    target_include_directories(ed2k_calc PRIVATE "${RHASH_INCLUDE_DIR}")
endif()
if(RHASH_LIBRARY)
    target_link_libraries(ed2k_calc PRIVATE "${RHASH_LIBRARY}")
endif()

if(MSVC)
    target_compile_options(ed2k_calc PRIVATE /W4)
else()
    target_compile_options(ed2k_calc PRIVATE -Wall -Wextra -O2)
endif()

option(CINEDECK_BUILD_TESTS "Build C unit and file tests" OFF)
if(CINEDECK_BUILD_TESTS)
    add_executable(test_ed2k test_ed2k.c)
    target_link_libraries(test_ed2k PRIVATE ed2k_calc)
    if(RHASH_LIBRARY)
        target_link_libraries(test_ed2k PRIVATE "${RHASH_LIBRARY}")
    endif()
endif()