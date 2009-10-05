TEMPLATE = lib
TARGET = QtScriptEditor
QT += script

include(../../qtcreatorplugin.pri)
include(../../plugins/coreplugin/coreplugin.pri)
include(../../plugins/texteditor/texteditor.pri)
include(../../shared/qscripthighlighter/qscripthighlighter.pri)
include(../../shared/indenter/indenter.pri)
include(parser/parser.pri)

HEADERS += qtscripteditor.h \
qtscripteditorfactory.h \
qtscripteditorplugin.h \
qtscripthighlighter.h \
qtscripteditoractionhandler.h \
qtscriptcodecompletion.h

SOURCES += qtscripteditor.cpp \
qtscripteditorfactory.cpp \
qtscripteditorplugin.cpp \
qtscripthighlighter.cpp \
qtscripteditoractionhandler.cpp \
qtscriptcodecompletion.cpp

RESOURCES += qtscripteditor.qrc
