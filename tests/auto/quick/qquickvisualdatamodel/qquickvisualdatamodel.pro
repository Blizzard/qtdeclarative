CONFIG += testcase
TARGET = tst_qquickvisualdatamodel
macx:CONFIG -= app_bundle

SOURCES += tst_qquickvisualdatamodel.cpp

include (../../shared/util.pri)
include (../shared/util.pri)

TESTDATA = data/*

CONFIG += parallel_test

QT += core-private gui-private v8-private qml-private quick-private widgets testlib