
# XR Game Bridge
> This project, initially started as a passion project, has spun-off from Leia Inc and is now standalone.
Thanks again to Leia Inc and former Dimenco for their support, we still strive to get SR in the hands of as many people as possible out of love for the technology.

## About
XR Game Bridge is an OpenXR runtime made to run UEVR supported games on Leia SR displays.
Supported displays include:
- Acer Spatiallabs compatible displays
- Samsung Odyssey 3D displays

If you want to play other games in 3D, see [3DGamebridgeProjects](https://github.com/JoeyAnthony/3DGameBridgeProjects).

If you need help or have any other questions, you can join our discord server: [join the community today](https://discord.gg/K46jgbzwDa).

## Compatibility
This runtime is mostly tested with `Returnal`, other UEVR supported games "should" work but they must be using DirectX12.
If you find a game with these requirements that doesn't work, please let me know.
A UEVR game compatibility list can be found in the [Flatscreen to VR](https://discord.com/invite/kxH7pnF6Ys) discord server.

## Setting up XRGameBridge with UEVR:
1. Go to the XRGameBridge folder and run `setruntime_release.bat` as administrator. This will set XRGameBridge as the current OpenXR runtime.
2. Download this version of uevr https://github.com/praydog/UEVR/issues/371#issuecomment-3316557682
3. Extract `UEVRBackend.dll` from the `UEVR my build_cache_fov_for_refresh.zip` file and replace it with the original version in the UEVR injector folder. If you don't do this you cannot tweak depth or pop-in/out.
4. Run `UEVRInjector.exe` as administrator.
5. Make sure the `OpenXR` checkbox is enabled in the UEVRInjector.
6. Start an Unreal Engine Game using DirectX12.
7. Make sure the game is open on a non SR display. The game only handles input from it's own window. Setting the game in windowed mode can be easier to test with.
8. In the UEVRInjector, select the game you want to inject to from the dropdown and press `inject`.
9. A new window should open on the SR screen with the game running in 3D.
10. Give focus to the original game window to start playing.

## Known issues
- On some screens, Only part of the game image is visible, as if it is zoomed in. This can be fixed by setting the display scaling to 100% in the Windows Display Settings.
- FOV related effects (like zoom) in games won't work (well) as the FOV is decided by the OpenXR Runtime, not by the game anymore.
- There ia a black flash, this can be very appearent on some systems.
- If the connected SR display was not detected by the SR runtime yet, the XRGameBridge window may open on a NON sr display. In this case, restart the game en inject again.

## How to build
The project was build with CMake and C++ 20 for Visual Studio 2022.

### Instructions
- Clone the repo with `git clone  https://github.com/JoeyAnthony/XRGameBridge.git`.
- Update submodules `git submodule update --init --recursive`.
- Run CMake for Visual Studio.
- Build the `RuntimeOpenXR` project.
