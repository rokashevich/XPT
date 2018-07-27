DEFINES += "VERSION=\"\\\"$$system(git describe --always)\\\"\""

TARGET = xpt
TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += $$PWD/include
LIBS += -L$$PWD/lib
win32{
LIBS += -llibcurl_a
}else{
LIBS += -lcurl -lboost_filesystem -lboost_system -lboost_regex
}

SOURCES += xpt.cpp
DESTDIR = $$PWD
