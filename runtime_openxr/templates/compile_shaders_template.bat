echo off

SET var=%~dp0

:: Enable Visual Studio Development consoles
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"

:: Compile DX12 shaders
dxc.exe -E main -Fo ${SHADER_DX12_VERTEX_OUTPUT} -T vs_6_0 -nologo ${SHADER_DX12_VERTEX_INPUT}
dxc.exe -E main -Fo ${SHADER_DX12_FRAGMENT_OUTPUT} -T ps_6_0 -nologo ${SHADER_DX12_FRAGMENT_INPUT}

:: Compile DX11 shaders
fxc /T vs_5_0 /Fo ${SHADER_DX11_VERTEX_OUTPUT} ${SHADER_DX11_VERTEX_INPUT}
fxc /T ps_5_0 /Fo ${SHADER_DX11_FRAGMENT_OUTPUT} ${SHADER_DX11_FRAGMENT_INPUT}

echo Finished compiling shaders
