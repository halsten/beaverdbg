IDE_BUILD_TREE = $$OUT_PWD/../../../

include(qworkbench.pri)
# include 'plugins' dirrectory
INCLUDEPATH	*= ${PWD}/..
# include 'core' dirrectory
INCLUDEPATH	*= ${PWD}/../../core

include(qworkbench.pri)
DESTDIR = $$IDE_LIBRARY_PATH

CONFIG -= shared
CONFIG *= staticlib

TARGET = $$qtLibraryTarget($$TARGET)
