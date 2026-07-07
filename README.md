# QtHoleMeasure

Qt demo for measuring square hole width and height with four LPV line gauges per hole.

## Run

Double-click `run_QtHoleMeasure.bat`.

The app loads these default files from `C:\Users\22113\Desktop\files`:

- `400P65H-image.bmp`
- `400P_6.5H_BALL_Template.csv`
- `distCharucoBoard_S_I.calib`

The saved result CSV is:

- `C:\Users\22113\Desktop\files\hole_measurements.csv`

## Build

From a VS2017 x64 developer prompt:

```bat
set PATH=C:\Qt\Qt5.9.4\5.9.4\msvc2017_64\bin;%PATH%
cd /d C:\Users\22113\Desktop\files\QtHoleMeasure
qmake QtHoleMeasure.pro -o Makefile
nmake
```
