include(../../qtcreator.pri)
include(../shared/qtsingleapplication/qtsingleapplication.pri)
include(../plugins/plugins.pri)
include(../libs/aggregation/aggregation.pri)
include(../libs/extensionsystem/extensionsystem.pri)
include(../libs/cplusplus/cplusplus.pri)
include(../libs/qtconcurrent/qtconcurrent.pri)
include(../libs/utils/utils.pri)

TEMPLATE = app
TARGET = $$IDE_APP_TARGET
TARGET = $$qtLibraryTarget($$TARGET)
DESTDIR = $$IDE_APP_PATH

PRE_TARGETDEPS *= ../plugins/projectexplorer ../libs

INCLUDEPATH *= ../libs/extensionsystem ../plugins/coreplugin  ../plugins

SOURCES += main.cpp

QT *= core gui script svg

#include(../rpath.pri)

win32 {
    RC_FILE = qtcreator.rc
} else:macx {
    ICON = beaverdbg.icns
} else {
	BEAVER_PREFIX=$$[BEAVER_PREFIX]
	BEAVER_LINK_PREFIX=$$[BEAVER_LINK_PREFIX]

	isEmpty( BEAVER_PREFIX ):BEAVER_PREFIX = /usr/local
	isEmpty( BEAVER_LINK_PREFIX ):BEAVER_LINK_PREFIX = /usr/
    
    message( "Beaverdbg binary will be installed to : $$BEAVER_PREFIX" )
    message( "Beaverdbg link will be installed to : $$BEAVER_LINK_PREFIX" )
    # binary
    target.path = $${BEAVER_PREFIX}/bin
    target.files	= $${DESTDIR}/$${IDE_APP_TARGET}
    target.CONFIG += no_check_exist

    # desktop file
    desktop.path	= $${BEAVER_LINK_PREFIX}/share/applications
    desktop.files	= beaverdbg.desktop

    # desktop icon file
    desktopicon.path	= $${BEAVER_PREFIX}/share/pixmaps
    desktopicon.files	= beaverdbg.png

    INSTALLS	+= target desktop desktopicon
}

OTHER_FILES += qtcreator.rc
