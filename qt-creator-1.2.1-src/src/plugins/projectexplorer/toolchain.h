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

#ifndef TOOLCHAIN_H
#define TOOLCHAIN_H

#include "environment.h"
#include <QtCore/QString>
#include <QtCore/QPair>

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT HeaderPath
{
public:
    enum Kind {
        GlobalHeaderPath,
        UserHeaderPath,
        FrameworkHeaderPath
    };

    HeaderPath()
        : _kind(GlobalHeaderPath)
    { }

    HeaderPath(const QString &path, Kind kind)
        : _path(path), _kind(kind)
    { }

    QString path() const { return _path; }
    Kind    kind() const { return _kind; }

private:
    QString _path;
    Kind _kind;
};



class PROJECTEXPLORER_EXPORT ToolChain
{
public:
    enum ToolChainType
    {
        GCC,
        LinuxICC,
        MinGW,
        MSVC,
        WINCE,
        OTHER,
        INVALID
    };

    virtual QByteArray predefinedMacros() = 0;
    virtual QList<HeaderPath> systemHeaderPaths() = 0;
    virtual void addToEnvironment(ProjectExplorer::Environment &env) = 0;
    virtual ToolChainType type() const = 0;
    virtual QString makeCommand() const = 0;

    ToolChain();
    virtual ~ToolChain();
    
    static bool equals(ToolChain *, ToolChain *);
    // Factory methods
    static ToolChain *createGccToolChain(const QString &gcc);
    static ToolChain *createMinGWToolChain(const QString &gcc, const QString &mingwPath);
    static ToolChain *createMSVCToolChain(const QString &name, bool amd64);
    static ToolChain *createWinCEToolChain(const QString &name, const QString &platform);
    static QStringList availableMSVCVersions();
    static QStringList supportedToolChains();

protected:
    virtual bool equals(ToolChain *other) const = 0;
};

namespace Internal {
class GccToolChain : public ToolChain
{
public:
    GccToolChain(const QString &gcc);
    virtual QByteArray predefinedMacros();
    virtual QList<HeaderPath> systemHeaderPaths();
    virtual void addToEnvironment(ProjectExplorer::Environment &env);
    virtual ToolChainType type() const;
    virtual QString makeCommand() const;

protected:
    virtual bool equals(ToolChain *other) const;
private:
    QString m_gcc;
    QByteArray m_predefinedMacros;
    QList<HeaderPath> m_systemHeaderPaths;
};

// TODO this class needs to fleshed out more
class MinGWToolChain : public GccToolChain
{
public:
    MinGWToolChain(const QString &gcc, const QString &mingwPath);
    virtual void addToEnvironment(ProjectExplorer::Environment &env);
    virtual ToolChainType type() const;
    virtual QString makeCommand() const;
protected:
    virtual bool equals(ToolChain *other) const;
private:
    QString m_mingwPath;
};

// TODO some stuff needs to be moved into this
class MSVCToolChain : public ToolChain
{
public:
    MSVCToolChain(const QString &name, bool amd64 = false);
    virtual QByteArray predefinedMacros();
    virtual QList<HeaderPath> systemHeaderPaths();
    virtual void addToEnvironment(ProjectExplorer::Environment &env);
    virtual ToolChainType type() const;
    virtual QString makeCommand() const;
protected:
    virtual bool equals(ToolChain *other) const;
    QString m_name;
private:
    mutable QList<QPair<QString, QString> > m_values;
    mutable bool m_valuesSet;
    mutable ProjectExplorer::Environment m_lastEnvironment;
    bool m_amd64;
};

// TODO some stuff needs to be moved into here
class WinCEToolChain : public MSVCToolChain
{
public:
    WinCEToolChain(const QString &name, const QString &platform);
    virtual QByteArray predefinedMacros();
    virtual QList<HeaderPath> systemHeaderPaths();
    virtual void addToEnvironment(ProjectExplorer::Environment &env);
    virtual ToolChainType type() const;
protected:
    virtual bool equals(ToolChain *other) const;
private:
    QString m_platform;
};

}
}

#endif // TOOLCHAIN_H
