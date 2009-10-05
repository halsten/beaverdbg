include(../../qtcreator.pri)
include(../shared/qtsingleapplication/qtsingleapplication.pri)

TEMPLATE = app
TARGET = $$IDE_APP_TARGET
DESTDIR = $$IDE_APP_PATH

SOURCES += main.cpp

include(../rpath.pri)

win32 {
    CONFIG(debug, debug|release):LIBS *= -lExtensionSystemd -lAggregationd
    else:LIBS *= -lExtensionSystem -lAggregation

    RC_FILE = qtcreator.rc
} else:macx {
    CONFIG(debug, debug|release):LIBS *= -lExtensionSystem_debug -lAggregation_debug
    else:LIBS *= -lExtensionSystem -lAggregation

    ICON = qtcreator.icns
    QMAKE_INFO_PLIST = Info.plist
} else {
    LIBS *= -lExtensionSystem -lAggregation

    target.path  = /bin
    INSTALLS    += target
}
