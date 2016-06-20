TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    v4l2SingleFrame.c \
    v4l2StrippedDown.c \
    singleFrame5Loops.c \
    singleFrame5Loops.cpp \
    grabSingleImageV4L2.cpp
