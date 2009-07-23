LIBS *=  -l$$qtLibraryTarget(CppEditor) -l$$qtLibraryTarget(CppTools ) \
	-l$$qtLibraryTarget(Debugger) -l$$qtLibraryTarget(ProjectExplorer) \
	-l$$qtLibraryTarget(TextEditor) -l$$qtLibraryTarget(Find) \
	-l$$qtLibraryTarget(QuickOpen) -l$$qtLibraryTarget(Core)

PRE_TARGETDEPS *= $$PWD/debugger $$PWD/texteditor $$PWD/find $$PWD/projectexplorer $$PWD/coreplugin

INCLUDEPATH *=  $$PWD/debugger

#hack for include to main.cpp ui_findwidget.h
INCLUDEPATH *=  $$PWD/find/.uic