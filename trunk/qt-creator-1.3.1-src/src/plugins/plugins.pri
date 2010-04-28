LIBS *=  	-L$$IDE_PLUGIN_PATH/Nokia \
	-l$$qtLibraryTarget(Debugger) \
	-l$$qtLibraryTarget(CppEditor) \
	-l$$qtLibraryTarget(CppTools ) \
	-l$$qtLibraryTarget(TextEditor)\
	-l$$qtLibraryTarget(Find) \
	-l$$qtLibraryTarget(Locator) \
	-l$$qtLibraryTarget(Core)

PRE_TARGETDEPS *= $$PWD/debugger $$PWD/texteditor $$PWD/find $$PWD/projectexplorer $$PWD/coreplugin

INCLUDEPATH *=  $$PWD/debugger

#hack for include to main.cpp ui_findwidget.h
INCLUDEPATH *=  $$PWD/find/.uic