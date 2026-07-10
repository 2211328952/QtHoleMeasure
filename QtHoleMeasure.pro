QT += core gui widgets winextras axcontainer

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = QtHoleMeasure
TEMPLATE = app
CONFIG += c++11

SOURCES += \
    src/main.cpp \
    src/HoleMeasureCore.cpp \
    src/LPVDisplayWidget.cpp \
    src/QtHoleMeasureWidget.cpp

HEADERS += \
    src/HoleMeasureCore.h \
    src/LPVDisplayWidget.h \
    src/QtHoleMeasureWidget.h \
    src/DisplayHelper.h

INCLUDEPATH += F:/Source/StoneWall/idl/GeneratedFiles \
    F:/Source/StoneWall/publish/include \
    F:/Source/StoneWall/include/capi

LIBS += -L$$PWD/lib/x64 \
    -LF:/Source/StoneWall/publish/lib/x64 \
    -luser32 -lgdi32 \
    -llpvCore -llpvGeom -llpvCalib -llpvGauge -llpvLocate -llpvDisplay -llpvIB

win32 {
    CONFIG(debug, debug|release): LPV_COPY_DEST = $$OUT_PWD/debug
    CONFIG(release, debug|release): LPV_COPY_DEST = $$OUT_PWD/release
    QMAKE_POST_LINK += $$quote(xcopy /Y /D $$shell_path($$PWD/release/*.dll) $$shell_path($$LPV_COPY_DEST) $$escape_expand(\\n\\t))
}
