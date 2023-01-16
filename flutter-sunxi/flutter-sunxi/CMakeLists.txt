cmake_minimum_required(VERSION 3.15)

# Build options.
option(USE_GLES3 "Use OpenGL ES3 (default is OpenGL ES2)" OFF)

############################################################
# Cross Compile
############################################################
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Specify the sysroot.
# You need to modify <path_to_user_target_sysroot> appropriately for your environment.
set(TARGET_SYSROOT "/home/anruliu/workspace/tools/r818/target")
set(FLUTTER_ENGINE "/home/anruliu/workspace/flutter-workspace/engine/src")
set(CMAKE_SYSROOT "/home/anruliu/workspace/tools/r818/toolchain")

set(ENV{PKG_CONFIG_PATH} "${TARGET_SYSROOT}/usr/lib/pkgconfig;${TARGET_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} ${TARGET_SYSROOT})

# Specify the cross compiler.
set(triple aarch64-linux-gnu)
set(CMAKE_C_COMPILER_TARGET ${triple})
set(CMAKE_C_COMPILER ${FLUTTER_ENGINE}/buildtools/linux-x64/clang/bin/clang)
set(CMAKE_CXX_COMPILER_TARGET ${triple})
set(CMAKE_CXX_COMPILER ${FLUTTER_ENGINE}/buildtools/linux-x64/clang/bin/clang++)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_FIND_ROOT_PATH ${TARGET_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

include_directories(${TARGET_SYSROOT}/usr/include)
link_directories(${TARGET_SYSROOT}/usr/lib)

project(FlutterEmbedderSUNXI)

############################################################
# OpenGL ES
############################################################
include_directories(drivers)
include_directories(linux_embedded)
set(EMBEDDER_COMMON_SRC
  "FlutterEmbedderSUNXI.cc"
  "drivers/evdev.cc"
  "linux_embedded/task_runner.cc"
  "linux_embedded/system_utils.cc"
)

if(USE_GLES3)
  set(FLUTTER_BIN flutter_eglfs)
  add_executable(${FLUTTER_BIN}
    ${EMBEDDER_COMMON_SRC}
    drivers/sunxiegl.cc
  )

  find_package(PkgConfig)
# common libraries.
  pkg_check_modules(EGL REQUIRED egl)
# requires for supporting external texture plugin.
# OpenGL ES3 are included in glesv2.
  pkg_check_modules(GLES REQUIRED glesv2)
  target_link_libraries(${FLUTTER_BIN} ${TARGET_SYSROOT}/usr/lib/libEGL.so)
  target_link_libraries(${FLUTTER_BIN} ${TARGET_SYSROOT}/usr/lib/libGLESv2.so)

  add_definitions(-DUSE_GLES3)
else()
  set(FLUTTER_BIN flutter_fbdev)
  add_executable(${FLUTTER_BIN}
    ${EMBEDDER_COMMON_SRC}
    drivers/sunxifb.cc
  )
endif()

############################################################
# Flutter Engine
############################################################
# This is assuming you've built a local version of the Flutter Engine.  If you
# downloaded yours is from the internet you'll have to change this.
include_directories(${FLUTTER_ENGINE}/flutter/shell/platform/embedder)
#find_library(FLUTTER_LIB flutter_engine PATHS ${FLUTTER_ENGINE}/out/linux_release_arm64)
set(FLUTTER_LIB "${FLUTTER_ENGINE}/out/linux_release_arm64/libflutter_engine.so")
target_link_libraries(${FLUTTER_BIN} ${FLUTTER_LIB})

# Copy the flutter library here since the shared library
# name is `./libflutter_engine.dylib`.
add_custom_command(
        TARGET ${FLUTTER_BIN} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${FLUTTER_LIB}
                ${CMAKE_CURRENT_BINARY_DIR})

# Strip binary for release builds
add_custom_command(TARGET ${FLUTTER_BIN} POST_BUILD
    COMMAND ${CMAKE_STRIP}
            --strip-all
            ${FLUTTER_BIN})