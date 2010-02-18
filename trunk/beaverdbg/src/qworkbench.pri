IDE_SOURCE_TREE = $$PWD/../

CONFIG -= release debug
CONFIG *= release

isEmpty(TEST):CONFIG(debug, debug|release) {
    !debug_and_release|build_pass {
        TEST = 1
    }
}

equals(TEST, 1) {
    QT +=testlib
    DEFINES += WITH_TESTS
}

isEmpty(IDE_BUILD_TREE) {
    error("qworkbench.pri: including file must define IDE_BUILD_TREE (probably a relative path)")
}
macx {
    IDE_APP_TARGET   = Beaver
    
    IDE_PLUGIN_PATH  = $$IDE_LIBRARY_PATH
	IDE_LIBRARY_PATH = $$IDE_BUILD_TREE/bin/$${IDE_APP_TARGET}.app/Contents/PlugIns
    contains(QT_CONFIG, ppc):CONFIG += ppc x86
} else {
    IDE_APP_TARGET   = beaverdbg
	IDE_LIBRARY_PATH = $$IDE_BUILD_TREE/lib/qtcreator
    IDE_PLUGIN_PATH  = $$IDE_LIBRARY_PATH/plugins/
}
IDE_APP_PATH = $$IDE_BUILD_TREE/bin
win32 {
    IDE_LIBRARY_PATH ~= s|/+|\|
    IDE_APP_PATH ~= s|/+|\|
}

defineReplace(qtLibraryTarget) {
	unset(Q_LIB_NAME)
	Q_LIB_NAME = $$1
	CONFIG(debug, debug|release) {
		!debug_and_release|build_pass {
			unix:RET = $$member(Q_LIB_NAME, 0)_debug
			else:win32:RET = $$member(Q_LIB_NAME, 0)d
		}
	}
	isEmpty(RET):RET = $$Q_LIB_NAME
	return($$RET)
}

INCLUDEPATH += \
    $$IDE_SOURCE_TREE/src/libs \
    $$IDE_SOURCE_TREE/tools \

DEPENDPATH += \
    $$IDE_SOURCE_TREE/src/libs \
    $$IDE_SOURCE_TREE/tools \

LIBS += -L$$IDE_LIBRARY_PATH

unix {
    debug:OBJECTS_DIR = $${OUT_PWD}/.obj/debug-shared
    release:OBJECTS_DIR = $${OUT_PWD}/.obj/release-shared

    debug:MOC_DIR = $${OUT_PWD}/.moc/debug-shared
    release:MOC_DIR = $${OUT_PWD}/.moc/release-shared

    RCC_DIR = $${OUT_PWD}/.rcc/
    UI_DIR = $${OUT_PWD}/.uic/
}