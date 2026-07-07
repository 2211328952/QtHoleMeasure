@echo off
setlocal
set "PATH=C:\Qt\Qt5.9.4\5.9.4\msvc2017_64\bin;F:\Source\StoneWall\publish\bin\x64;%PATH%"
cd /d "%~dp0release"
start "" "QtHoleMeasure.exe"
