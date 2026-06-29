@echo off
set UE_EDITOR=E:\Unreal\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe
set PROJECT=E:\Unreal\git\SpaCh4_Copy\SpaCh4.uproject
set SCRIPT=%~dp0BuildHUDWidgets.py

"%UE_EDITOR%" "%PROJECT%" -ExecutePythonScript="%SCRIPT%" -log
