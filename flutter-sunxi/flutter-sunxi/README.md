# Flutter Embedder Engine SUNXI

## Description
This is the source code that flutter can run on the sunxi platform.
For more information about using the embedder you can read the wiki article [Custom Flutter Engine Embedders](https://github.com/flutter/flutter/wiki/Custom-Flutter-Engine-Embedders).

## Running Instructions
The code has the following dependencies:
 * [CMake](https://cmake.org/) - This can be installed with [Homebrew](https://brew.sh/) - `brew install cmake`
 * [Flutter](https://flutter.dev/) - This can be installed from the [Flutter webpage](https://flutter.dev/docs/get-started/install)
 * [Flutter Engine](https://flutter.dev) - This can be built or downloaded, see [Custom Flutter Engine Embedders](https://github.com/flutter/flutter/wiki/Custom-Flutter-Engine-Embedders) for more information.

In order to **build** the code you should be able to go into this directory and run
`cd ./script
`./compile_sunxi.sh`.

## Troubleshooting
There are a few things you might have to tweak in order to get your build working:
 * Flutter Engine Location - Inside the `CMakeList.txt` file you will see that it is set up to search for the header and library for the Flutter Engine in specific locations, those might not be the location of your Flutter Engine.You can set flutter_engine.
 * target_sysroot - Inside the `CMakeList.txt` file set target_sysroot to ensure that the library header file can be found.
 * CMAKE_SYSROOT - Inside the `CMakeList.txt` file set CMAKE_SYSROOT to ensure that the toolchain path is found.

## Directory tree
```
.
├── flutter-workspace
│   ├── depot_tools
│   ├── engine
│   ├── flutter
│   ├── flutter-pi
│   ├── flutter-sunxi
│   │   ├── flutter-embedded-linux
│   │   └── flutter-sunxi
│   ├── gallery
│   └── plugins
├── open-harmony
└── tools
    ├── cmake-3.21.1-linux-x86_64
    ├── cmake-3.21.1-linux-x86_64.tar.gz
    ├── flutter-workspace.tar.gz
    ├── ninja
    ├── r18
    └── r818
```

## flutter-embedded-linux
In the flutter-embedded-linux directory, you need to see the original submission, you can execute
```
mv .mygit .git
```