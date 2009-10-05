TEMPLATE = lib
TARGET = Utils
QT += gui \
    network
DEFINES += QTCREATOR_UTILS_LIBRARY
include(../../qtcreatorlibrary.pri)
SOURCES += reloadpromptutils.cpp \
    settingsutils.cpp \
    filesearch.cpp \
    pathchooser.cpp \
    pathlisteditor.cpp \
    filewizardpage.cpp \
    filewizarddialog.cpp \
    projectintropage.cpp \
    basevalidatinglineedit.cpp \
    filenamevalidatinglineedit.cpp \
    projectnamevalidatinglineedit.cpp \
    codegeneration.cpp \
    newclasswidget.cpp \
    classnamevalidatinglineedit.cpp \
    linecolumnlabel.cpp \
    fancylineedit.cpp \
    qtcolorbutton.cpp \
    savedaction.cpp \
    submiteditorwidget.cpp \
    synchronousprocess.cpp \
    submitfieldwidget.cpp \
    consoleprocess.cpp \
    uncommentselection.cpp \
    parameteraction.cpp \
    treewidgetcolumnstretcher.cpp
win32 { 
    SOURCES += abstractprocess_win.cpp \
        consoleprocess_win.cpp \
        winutils.cpp
    HEADERS += winutils.h
}
else:SOURCES += consoleprocess_unix.cpp
HEADERS += utils_global.h \
    reloadpromptutils.h \
    settingsutils.h \
    filesearch.h \
    listutils.h \
    pathchooser.h \
    pathlisteditor.h \
    filewizardpage.h \
    filewizarddialog.h \
    projectintropage.h \
    basevalidatinglineedit.h \
    filenamevalidatinglineedit.h \
    projectnamevalidatinglineedit.h \
    codegeneration.h \
    newclasswidget.h \
    classnamevalidatinglineedit.h \
    linecolumnlabel.h \
    fancylineedit.h \
    qtcolorbutton.h \
    savedaction.h \
    submiteditorwidget.h \
    abstractprocess.h \
    consoleprocess.h \
    synchronousprocess.h \
    submitfieldwidget.h \
    uncommentselection.h \
    parameteraction.h \
    treewidgetcolumnstretcher.h
FORMS += filewizardpage.ui \
    projectintropage.ui \
    newclasswidget.ui \
    submiteditorwidget.ui
RESOURCES += utils.qrc
