IDE_BUILD_TREE = $$OUT_PWD/../../

include(../qworkbench.pri)
include(../plugins/plugins.pri)
include(../libs/aggregation/aggregation.pri)
include(../libs/extensionsystem/extensionsystem.pri)
include(../libs/cplusplus/cplusplus.pri)
include(../libs/qtconcurrent/qtconcurrent.pri)
include(../libs/utils/utils.pri)

PRE_TARGETDEPS *= ../plugins/projectexplorer ../libs

INCLUDEPATH *= ../libs/extensionsystem ../plugins/coreplugin  ../plugins

TEMPLATE = app
TARGET = $$IDE_APP_TARGET
TARGET = $$qtLibraryTarget($$TARGET)
DESTDIR = ../../bin

QT *= core gui script xml webkit svg
CONFIG *= help

SOURCES += main.cpp

include(../rpath.pri)

win32 {
        RC_FILE = qtcreator.rc
}

macx {
        ICON = qtcreator.icns
}
