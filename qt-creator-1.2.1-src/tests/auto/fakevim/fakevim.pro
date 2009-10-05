
QT += testlib

FAKEVIMDIR = ../../../src/plugins/fakevim
UTILSDIR = ../../../src/libs

SOURCES += \
	main.cpp \
	$$FAKEVIMDIR/fakevimhandler.cpp \
	$$FAKEVIMDIR/fakevimactions.cpp \
	$$UTILSDIR/utils/savedaction.cpp \
	$$UTILSDIR/utils/pathchooser.cpp \
	$$UTILSDIR/utils/basevalidatinglineedit.cpp \

HEADERS += \
	$$FAKEVIMDIR/fakevimhandler.h \
	$$FAKEVIMDIR/fakevimactions.h \
	$$UTILSDIR/utils/savedaction.h \
	$$UTILSDIR/utils/pathchooser.h \
	$$UTILSDIR/utils/basevalidatinglineedit.h \

INCLUDEPATH += $$FAKEVIMDIR $$UTILSDIR

