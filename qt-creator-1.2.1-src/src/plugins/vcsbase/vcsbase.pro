TEMPLATE = lib
TARGET = VCSBase
DEFINES += VCSBASE_LIBRARY
include(../../qtcreatorplugin.pri)
include(vcsbase_dependencies.pri)
HEADERS += vcsbase_global.h \
    vcsbaseconstants.h \
    vcsbaseplugin.h \
    baseannotationhighlighter.h \
    diffhighlighter.h \
    vcsbasetextdocument.h \
    vcsbaseeditor.h \
    vcsbasesubmiteditor.h \
    basevcseditorfactory.h \
    submiteditorfile.h \
    basevcssubmiteditorfactory.h \
    submitfilemodel.h \
    vcsbasesettings.h \
    vcsbasesettingspage.h \
    nicknamedialog.h

SOURCES += vcsbaseplugin.cpp \
    baseannotationhighlighter.cpp \
    diffhighlighter.cpp \
    vcsbasetextdocument.cpp \
    vcsbaseeditor.cpp \
    vcsbasesubmiteditor.cpp \
    basevcseditorfactory.cpp \
    submiteditorfile.cpp \
    basevcssubmiteditorfactory.cpp \
    submitfilemodel.cpp \
    vcsbasesettings.cpp \
    vcsbasesettingspage.cpp \
    nicknamedialog.cpp

RESOURCES += vcsbase.qrc

FORMS += vcsbasesettingspage.ui \
    nicknamedialog.ui

OTHER_FILES += VCSBase.pluginspec
