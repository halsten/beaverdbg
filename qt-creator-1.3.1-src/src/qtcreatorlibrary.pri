include(../qtcreator.pri)

win32 {
    DLLDESTDIR = $$IDE_APP_PATH
}

CONFIG -= shared dll dylib
CONFIG *= staticlib
CONFIG -= release debug_and_release
CONFIG *= debug

DESTDIR = $$IDE_LIBRARY_PATH

include(rpath.pri)

TARGET = $$qtLibraryTarget($$TARGET)

contains(QT_CONFIG, reduce_exports):CONFIG += hide_symbols

!macx {
    win32 {
        target.path = /bin
        target.files = $$DESTDIR/$${TARGET}.dll
    } else {
        target.path = /$$IDE_LIBRARY_BASENAME/qtcreator
    }
    INSTALLS += target
}
