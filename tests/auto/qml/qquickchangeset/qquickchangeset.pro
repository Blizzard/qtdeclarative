CONFIG += testcase
TARGET = tst_qquickchangeset
macx:CONFIG -= app_bundle

SOURCES += tst_qquickchangeset.cpp

CONFIG += parallel_test

QT += core-private gui-private qml-private quick-private testlib