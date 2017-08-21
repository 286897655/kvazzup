#-------------------------------------------------
#
# Project created by QtCreator 2016-04-07T09:05:00
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = HEVC_conference
TEMPLATE = app

INCLUDEPATH += src

SOURCES +=\
    src/callwindow.cpp \
    src/camerafilter.cpp \
    src/cameraframegrabber.cpp \
    src/displayfilter.cpp \
    src/filter.cpp \
    src/filtergraph.cpp \
    src/main.cpp \
    src/videowidget.cpp \
    src/kvazaarfilter.cpp \
    src/rgb32toyuv.cpp \
    src/openhevcfilter.cpp \
    src/yuvtorgb32.cpp \
    src/rtpstreamer.cpp \
    src/framedsourcefilter.cpp \
    src/rtpsinkfilter.cpp \
    src/audiocapturefilter.cpp \
    src/audiocapturedevice.cpp \
    src/statisticswindow.cpp \
    src/audiooutput.cpp \
    src/audiooutputdevice.cpp \
    src/opusencoderfilter.cpp \
    src/opusdecoderfilter.cpp \
    src/speexaecfilter.cpp \
    src/callnegotiation.cpp \
    src/sipstringcomposer.cpp \
    src/connection.cpp \
    src/connectionserver.cpp \
    src/sipparser.cpp \
    src/common.cpp \
    src/mediamanager.cpp \
    src/conferenceview.cpp \
    src/settings.cpp \
    src/contactlist.cpp \
    src/contactlistitem.cpp \
    src/dshowcamerafilter.cpp \
    src/dshow/capture.cpp \
    src/callmanager.cpp

HEADERS  += \
    src/callwindow.h \
    src/camerafilter.h \
    src/cameraframegrabber.h \
    src/displayfilter.h \
    src/filter.h \
    src/filtergraph.h \
    src/videowidget.h \
    src/kvazaarfilter.h \
    src/rgb32toyuv.h \
    src/openhevcfilter.h \
    src/yuvtorgb32.h \
    src/rtpstreamer.h \
    src/framedsourcefilter.h \
    src/rtpsinkfilter.h \
    src/audiocapturefilter.h \
    src/audiocapturedevice.h \
    src/statisticswindow.h \
    src/statisticsinterface.h \
    src/audiooutput.h \
    src/audiooutputdevice.h \
    src/opusencoderfilter.h \
    src/opusdecoderfilter.h \
    src/speexaecfilter.h \
    src/callnegotiation.h \
    src/sipstringcomposer.h \
    src/connection.h \
    src/connectionserver.h \
    src/sipparser.h \
    src/common.h \
    src/mediamanager.h \
    src/conferenceview.h \
    src/settings.h \
    src/contactlist.h \
    src/contactlistitem.h \
    src/participantinterface.h \
    src/dshowcamerafilter.h \
    src/dshow/capture.h \
    src/dshow/capture_interface.h \
    src/dshow/SampleGrabber.h \
    src/optimized/rgb2yuv.h \
    src/optimized/yuv2rgb.h \
    src/callmanager.h

FORMS    += \
    ui/callwindow.ui \
    ui/statisticswindow.ui \
    ui/callingwidget.ui \
    ui/about.ui \
    ui/advancedSettings.ui \
    ui/basicSettings.ui


QT+=multimedia
QT+=multimediawidgets
QT+=network

QMAKE_CXXFLAGS += -std=c++11
QMAKE_CXXFLAGS += -msse4.1 -mavx2

CONFIG += console

INCLUDEPATH += $$PWD/../include/openhevc_dec
INCLUDEPATH += $$PWD/../include/opus
INCLUDEPATH += $$PWD/../include/live/liveMedia/include
INCLUDEPATH += $$PWD/../include/live/groupsock/include
INCLUDEPATH += $$PWD/../include/live/UsageEnvironment/include
INCLUDEPATH += $$PWD/../include/live/BasicUsageEnvironment/include
INCLUDEPATH += $$PWD/../include/

win32: LIBS += -L$$PWD/../libs
win32: LIBS += -llibkvazaar.dll
win32: LIBS += -llibopus.dll
win32: LIBS += -llibLibOpenHevcWrapper.dll
win32: LIBS += -llivemedia.dll
win32: LIBS += -llibspeexdsp.dll
win32: LIBS += -lws2_32

win32: LIBS += -lstrmiids
win32: LIBS += -lole32
win32: LIBS += -loleaut32


INCLUDEPATH += $$PWD/../
DEPENDPATH += $$PWD/../

DISTFILES += \
    .gitignore
