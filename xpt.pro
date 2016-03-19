DEFINES += "VERSION=\"\\\"$$system(git describe --always)\\\"\""

TARGET = xpt
TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += xpt.cpp
DESTDIR = $$PWD
