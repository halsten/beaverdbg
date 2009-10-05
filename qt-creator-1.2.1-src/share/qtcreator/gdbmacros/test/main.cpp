/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://www.qtsoftware.com/contact.
**
**************************************************************************/

#include <QtCore/QStringList>
#include <QtCore/QVector>
#include <QtCore/QTimer>

#include <string>
#include <list>
#include <vector>
#include <set>

#include <stdio.h>
#include <string.h>

// Test uninitialized variables allocing memory
bool optTestUninitialized = false;

template <class T>
        inline T* testAddress(T* in)
{
    return optTestUninitialized ?
        (reinterpret_cast<T*>(new char[sizeof(T)]))
        : in;
}

/* Test program for Dumper development/porting.
 * Takes the type as first argument. */

// --------------- Dumper symbols
extern char qDumpInBuffer[10000];
extern char qDumpOutBuffer[100000];

extern "C" void *qDumpObjectData440(
    int protocolVersion,
    int token,
    void *data,
#ifdef Q_CC_MSVC // CDB cannot handle boolean parameters
    int dumpChildren,
#else
    bool dumpChildren,
#endif
    int extraInt0, int extraInt1, int extraInt2, int extraInt3);

static void prepareInBuffer(const char *outerType,
                            const char *iname,
                            const char *expr,
                            const char *innerType)
{
    // Leave trailing '\0'
    char *ptr = qDumpInBuffer;
    strcpy(ptr, outerType);
    ptr += strlen(outerType);
    ptr++;
    strcpy(ptr, iname);
    ptr += strlen(iname);
    ptr++;
    strcpy(ptr, expr);
    ptr += strlen(expr);
    ptr++;
    strcpy(ptr, innerType);
    ptr += strlen(innerType);
    ptr++;
    strcpy(ptr, iname);
}

// ---------------  Qt types
static int dumpQString()
{
    QString test = QLatin1String("hallo");
    prepareInBuffer("QString", "local.qstring", "local.qstring", "");
    qDumpObjectData440(2, 42, testAddress(&test), 1, 0, 0, 0, 0);
    fputs(qDumpOutBuffer, stdout);
    fputc('\n', stdout);
    QString uninitialized;
    return 0;
}

static int dumpQStringList()
{
    QStringList test = QStringList() << QLatin1String("item1") << QLatin1String("item2");
    prepareInBuffer("QList", "local.qstringlist", "local.qstringlist", "QString");
    qDumpObjectData440(2, 42, testAddress(&test), 1, sizeof(QString), 0, 0, 0);
    fputs(qDumpOutBuffer, stdout);
    fputc('\n', stdout);
    return 0;
}

static int dumpQIntList()
{
    QList<int> test = QList<int>() << 1 << 2;
    prepareInBuffer("QList", "local.qintlist", "local.qintlist", "int");
    qDumpObjectData440(2, 42, testAddress(&test), 1, sizeof(int), 0, 0, 0);
    fputs(qDumpOutBuffer, stdout);
    fputc('\n', stdout);
    return 0;
}

static int dumpQIntVector()
{
    QVector<int> test = QVector<int>() << 1 << 2;
    prepareInBuffer("QVector", "local.qintvector", "local.qintvector", "int");
    qDumpObjectData440(2, 42, testAddress(&test), 1, sizeof(int), 0, 0, 0);
    fputs(qDumpOutBuffer, stdout);
    fputc('\n', stdout);
    return 0;
}

// ---------------  std types

static int dumpStdString()
{
    std::string test = "hallo";
    prepareInBuffer("std::string", "local.string", "local.string", "");
    qDumpObjectData440(2, 42, testAddress(&test), 1, 0, 0, 0, 0);
    fputs(qDumpOutBuffer, stdout);
    fputc('\n', stdout);
    return 0;
}

static int dumpStdWString()
{
    std::wstring test = L"hallo";
    prepareInBuffer("std::wstring", "local.wstring", "local.wstring", "");
    qDumpObjectData440(2, 42, testAddress(&test), 1, 0, 0, 0, 0);
    fputs(qDumpOutBuffer, stdout);
    fputc('\n', stdout);
    return 0;
}

static int dumpStdStringList()
{
    std::list<std::string> test;
    test.push_back("item1");
    test.push_back("item2");
    prepareInBuffer("std::list", "local.stringlist", "local.stringlist", "std::string");
    qDumpObjectData440(2, 42, testAddress(&test), 1, sizeof(std::string), sizeof(std::list<std::string>::allocator_type), 0, 0);
    fputs(qDumpOutBuffer, stdout);
    fputc('\n', stdout);
    return 0;
}

static int dumpStdStringQList()
{
    QList<std::string> test;
    test.push_back("item1");
    test.push_back("item2");
    prepareInBuffer("QList", "local.stringqlist", "local.stringqlist", "std::string");
    qDumpObjectData440(2, 42, testAddress(&test), 1, sizeof(std::string), 0, 0, 0);
    fputs(qDumpOutBuffer, stdout);
    fputc('\n', stdout);
    return 0;
}

static int dumpStdIntList()
{
    std::list<int> test;
    test.push_back(1);
    test.push_back(2);
    prepareInBuffer("std::list", "local.intlist", "local.intlist", "int");
    qDumpObjectData440(2, 42, testAddress(&test), 1, sizeof(int), sizeof(std::list<int>::allocator_type), 0, 0);
    fputs(qDumpOutBuffer, stdout);
    fputc('\n', stdout);
    return 0;
}

static int dumpStdIntVector()
{
    std::vector<int> test;
    test.push_back(1);
    test.push_back(2);
    prepareInBuffer("std::vector", "local.intvector", "local.intvector", "int");
    qDumpObjectData440(2, 42, testAddress(&test), 1, sizeof(int), sizeof(std::list<int>::allocator_type), 0, 0);
    fputs(qDumpOutBuffer, stdout);
    fputc('\n', stdout);
    return 0;
}

static int dumpStdStringVector()
{
    std::vector<std::string> test;
    test.push_back("item1");
    test.push_back("item2");
    prepareInBuffer("std::vector", "local.stringvector", "local.stringvector", "std::string");
    qDumpObjectData440(2, 42, testAddress(&test), 1, sizeof(std::string), sizeof(std::list<int>::allocator_type), 0, 0);
    fputs(qDumpOutBuffer, stdout);
    fputc('\n', stdout);
    return 0;
}

static int dumpStdIntSet()
{
    std::set<int> test;
    test.insert(1);
    test.insert(2);
    prepareInBuffer("std::set", "local.intset", "local.intset", "int");
    qDumpObjectData440(2, 42, testAddress(&test), 1, sizeof(int), sizeof(std::list<int>::allocator_type), 0, 0);
    fputs(qDumpOutBuffer, stdout);
    fputc('\n', stdout);
    return 0;
}

static int dumpStdStringSet()
{
    std::set<std::string> test;
    test.insert("item1");
    test.insert("item2");
    prepareInBuffer("std::set", "local.stringset", "local.stringset", "std::string");
    qDumpObjectData440(2, 42, testAddress(&test), 1, sizeof(std::string), sizeof(std::list<int>::allocator_type), 0, 0);
    fputs(qDumpOutBuffer, stdout);
    fputc('\n', stdout);
    return 0;
}

static int dumpQObject()
{
    // Requires the childOffset to be know, but that is not critical
    QTimer t;
    prepareInBuffer("QObject", "local.qobject", "local.qobject", "");
    qDumpObjectData440(2, 42, testAddress(&t), 1, 0, 0, 0, 0);
    fputs(qDumpOutBuffer, stdout);
    fputc('\n', stdout);
    return 0;
}

int main(int argc, char *argv[])
{
    printf("Running query protocol\n");
    qDumpObjectData440(1, 42, 0, 1, 0, 0, 0, 0);
    fputs(qDumpOutBuffer, stdout);
    fputc('\n', stdout);
    fputc('\n', stdout);
    if (argc < 2)
        return 0;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (!strcmp(arg, "-u")) {
            optTestUninitialized = true;
            printf("\nTesting uninitialized...\n");
            continue;
        }
        printf("\nTesting %s\n", arg);
        if (!qstrcmp(arg, "QString"))
            dumpQString();
        if (!qstrcmp(arg, "QStringList"))
            dumpQStringList();
        if (!qstrcmp(arg, "QList<int>"))
            dumpQIntList();
        if (!qstrcmp(arg, "QList<std::string>"))
            dumpStdStringQList();
        if (!qstrcmp(arg, "QVector<int>"))
            dumpQIntVector();
        if (!qstrcmp(arg, "string"))
            dumpStdString();
        if (!qstrcmp(arg, "wstring"))
            dumpStdWString();
        if (!qstrcmp(arg, "list<int>"))
            dumpStdIntList();
        if (!qstrcmp(arg, "list<string>"))
            dumpStdStringList();
        if (!qstrcmp(arg, "vector<int>"))
            dumpStdIntVector();
        if (!qstrcmp(arg, "vector<string>"))
            dumpStdStringVector();
        if (!qstrcmp(arg, "set<int>"))
            dumpStdIntSet();
        if (!qstrcmp(arg, "set<string>"))
            dumpStdStringSet();
        if (!qstrcmp(arg, "QObject"))
            dumpQObject();
    }
    return 0;
}
