TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
TARGET = CoreTests

SOURCES += \
    CoreTests.cpp \
    ../src/HoleMeasureCore.cpp

HEADERS += \
    ../src/HoleMeasureCore.h
