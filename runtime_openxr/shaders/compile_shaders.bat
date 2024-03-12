echo off

SET var=%~dp0

call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"

dxc.exe -E main -Fo %var%layering_pixel.cso 	-T ps_6_0 -nologo %var%layering_pixel.hlsl

dxc.exe -E main -Fo %var%layering_vertex.cso -T vs_6_0 -nologo %var%layering_vertex.hlsl

echo Finished compiling shaders
pause