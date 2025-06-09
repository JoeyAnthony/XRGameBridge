echo off

SET var=%~dp0

call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"

@REM dxc.exe -E main -Fo %var%dx12.vs -T vs_6_0 -nologo %var%layering_vertex.hlsl
@REM dxc.exe -E main -Fo %var%dx12.ps -T ps_6_0 -nologo %var%layering_pixel.hlsl

fxc /T vs_5_0 /Fo %var%dx11.vs %var%dx11_vs.hlsl
fxc /T ps_5_0 /Fo %var%dx11.ps %var%dx11_ps.hlsl

echo Finished compiling shaders
pause
