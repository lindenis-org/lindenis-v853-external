cmake_minimum_required(VERSION 3.10)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Specify the sysroot.
# You need to modify <path_to_user_target_sysroot> appropriately for your environment.
set(TARGET_STSROOT "/home/anruliu/workspace/tools/r818/target")
set(FLUTTER_ENGINE "/home/anruliu/workspace/flutter-workspace/engine/src")
set(CMAKE_SYSROOT "/home/anruliu/workspace/tools/r818/toolchain")

set(ENV{PKG_CONFIG_PATH} "${TARGET_STSROOT}/usr/lib/pkgconfig;${TARGET_STSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} ${TARGET_STSROOT})

# Specify the cross compiler.
set(triple aarch64-linux-gnu)
set(CMAKE_C_COMPILER_TARGET ${triple})
set(CMAKE_C_COMPILER ${FLUTTER_ENGINE}/buildtools/linux-x64/clang/bin/clang)
set(CMAKE_CXX_COMPILER_TARGET ${triple})
set(CMAKE_CXX_COMPILER ${FLUTTER_ENGINE}/buildtools/linux-x64/clang/bin/clang++)

set(CMAKE_FIND_ROOT_PATH ${TARGET_STSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

link_directories(${TARGET_STSROOT}/usr/lib)
link_libraries(${CMAKE_SYSROOT}/lib/libdl.so)