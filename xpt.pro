DEFINES += "VERSION=\"\\\"$$system(git describe --always)\\\"\""

TARGET = xpt
TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += $$PWD\include
LIBS += -L$$PWD\lib -llibcurl_a

SOURCES += xpt.cpp
DESTDIR = $$PWD
