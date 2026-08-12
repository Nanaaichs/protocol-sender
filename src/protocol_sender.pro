QT += core gui widgets sql xml network

CONFIG += c++11 no_moc_predefs
TEMPLATE = app
TARGET = protocol_sender

SOURCES += \
    datagenerator.cpp \
    main.cpp \
    mainwindow.cpp \
    protocolparser.cpp \
    transmissionrepository.cpp \
    udpsendcontroller.cpp

HEADERS += \
    datagenerator.h \
    mainwindow.h \
    protocolparser.h \
    transmissionrepository.h \
    udpsendcontroller.h

RESOURCES += resources.qrc
