# XR Game Bridge
OpenXR Runtime meant run games modded with UEVR on SR displays

## How to build
The project builds with CMake and C++ 20 for Visual Studio 2022

### Instructions
- Clone the repo with `git clone  https://github.com/JoeyAnthony/XRGameBridge.git`
- Update submodules `git submodule update --init --recursive`
- Run CMake for Visual Studio
- Build the `RuntimeOpenXR` project

## Debugging
Right now the application only runs with hello_xr from the official Open XR source.
[Link](https://github.com/KhronosGroup/OpenXR-SDK-Source)

In Visual Studio, go to properties->debugging of the `RuntimeOpenXR` project.
Then add the full path to `hello_xr.exe` in the `Command` field, and in the arguments field use `-g d3d12`.

The runtime can be activated with one of the scripts inside `./runtime-openxr` for the respective build targets. The scripts should be run as administrator as it changes the registry.
