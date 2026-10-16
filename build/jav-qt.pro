CONFIG += c++20
TEMPLATE = app
TARGET = jav-qt
INCLUDEPATH += ../src
HEADERS += ../src/network.hpp ../src/window.hpp ../src/options.hpp ../src/models/video.hpp
SOURCES += ../src/main.cpp ../src/network.cpp ../src/window.cpp ../src/options.cpp
QT += widgets network
