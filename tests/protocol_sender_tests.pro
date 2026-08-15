QT += core gui widgets testlib sql xml network

CONFIG += c++11 console testcase no_moc_predefs
TEMPLATE = app
TARGET = protocol_sender_tests

SOURCES += \
    test_protocol_sender.cpp \
    ../src/datagenerator.cpp \
    ../src/mainwindow.cpp \
    ../src/protocolparser.cpp \
    ../src/transmissionrepository.cpp \
    ../src/udpsendcontroller.cpp

HEADERS += \
    ../src/datagenerator.h \
    ../src/mainwindow.h \
    ../src/protocolparser.h \
    ../src/transmissionrepository.h \
    ../src/udpsendcontroller.h

RESOURCES += ../src/resources.qrc
