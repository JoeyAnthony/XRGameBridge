echo off

SET var=%~dp0
reg add "HKLM\SOFTWARE\Khronos\OpenXR\1" /f /v "ActiveRuntime" /t "REG_EXPAND_SZ" /d "%var%XRGameBridge_DEBUG.json"

pause
