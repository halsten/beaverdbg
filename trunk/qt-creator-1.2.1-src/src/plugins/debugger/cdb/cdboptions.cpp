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

#include "cdboptions.h"

#include <QtCore/QSettings>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>

static const char *settingsGroupC = "CDB";
static const char *enabledKeyC = "Enabled";
static const char *pathKeyC = "Path";
static const char *symbolPathsKeyC = "SymbolPaths";
static const char *sourcePathsKeyC = "SourcePaths";

namespace Debugger {
namespace Internal {

CdbOptions::CdbOptions() :
    enabled(false)
{
}

void CdbOptions::clear()
{
    enabled = false;
    path.clear();
}

void CdbOptions::fromSettings(const QSettings *s)
{
    clear();
    // Is this the first time we are called ->
    // try to find automatically
    const QString keyRoot = QLatin1String(settingsGroupC) + QLatin1Char('/');
    const QString enabledKey = keyRoot + QLatin1String(enabledKeyC);
    const bool firstTime = !s->contains(enabledKey);
    if (firstTime) {
        enabled = autoDetectPath(&path);
        return;
    }
    enabled = s->value(enabledKey, false).toBool();
    path = s->value(keyRoot + QLatin1String(pathKeyC), QString()).toString();
    symbolPaths = s->value(keyRoot + QLatin1String(symbolPathsKeyC), QStringList()).toStringList();
    sourcePaths = s->value(keyRoot + QLatin1String(sourcePathsKeyC), QStringList()).toStringList();
}

void CdbOptions::toSettings(QSettings *s) const
{
    s->beginGroup(QLatin1String(settingsGroupC));
    s->setValue(QLatin1String(enabledKeyC), enabled);
    s->setValue(QLatin1String(pathKeyC), path);
    s->setValue(QLatin1String(symbolPathsKeyC), symbolPaths);
    s->setValue(QLatin1String(sourcePathsKeyC), sourcePaths);
    s->endGroup();
}

bool CdbOptions::autoDetectPath(QString *outPath, QStringList *checkedDirectories /* = 0 */)
{
    // Look for $ProgramFiles/"Debugging Tools For Windows <bit-idy>" and its
    // " (x86)", " (x64)" variations. Qt Creator needs 64/32 bit depending
    // on how it was built.
#ifdef Q_OS_WIN64
    static const char *postFixes[] = {" (x64)", " 64-bit" };
#else
    static const char *postFixes[] = { " (x86)", " (x32)" };
#endif

    outPath->clear();
    const QByteArray programDirB = qgetenv("ProgramFiles");
    if (programDirB.isEmpty())
        return false;

    const QString programDir = QString::fromLocal8Bit(programDirB) + QDir::separator();
    const QString installDir = QLatin1String("Debugging Tools For Windows");

    QString path = programDir + installDir;
    if (checkedDirectories)
        checkedDirectories->push_back(path);
    if (QFileInfo(path).isDir()) {
        *outPath = QDir::toNativeSeparators(path);
        return true;
    }
    const int rootLength = path.size();
    for (int i = 0; i < sizeof(postFixes)/sizeof(const char*); i++) {
        path.truncate(rootLength);
        path += QLatin1String(postFixes[i]);
        if (checkedDirectories)
            checkedDirectories->push_back(path);
        if (QFileInfo(path).isDir()) {
            *outPath = QDir::toNativeSeparators(path);
            return true;
        }
    }
    return false;
}

unsigned CdbOptions::compare(const CdbOptions &rhs) const
{
    unsigned rc = 0;
    if (enabled != rhs.enabled || path != rhs.path)
        rc |= InitializationOptionsChanged;
    if (symbolPaths != rhs.symbolPaths || sourcePaths != rhs.sourcePaths)
        rc |= DebuggerPathsChanged;
    return rc;
}

} // namespace Internal
} // namespace Debugger
