CONFIG += testcase
TARGET = tst_qquickitem2
macx:CONFIG -= app_bundle

SOURCES += tst_qquickitem.cpp

include (../../shared/util.pri)

TESTDATA = data/*

CONFIG += parallel_test

QT += core-private gui-private v8-private qml-private quick-private opengl-private testlib