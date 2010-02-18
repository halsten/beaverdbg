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
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#ifndef CVSCONTROL_H
#define CVSCONTROL_H

#include <coreplugin/iversioncontrol.h>

namespace CVS {
namespace Internal {

class CVSPlugin;

// Just a proxy for CVSPlugin
class CVSControl : public Core::IVersionControl
{
    Q_OBJECT
public:
    explicit CVSControl(CVSPlugin *plugin);
    virtual QString name() const;

    virtual bool isEnabled() const;
    virtual void setEnabled(bool enabled);

    virtual bool managesDirectory(const QString &directory) const;
    virtual QString findTopLevelForDirectory(const QString &directory) const;

    virtual bool supportsOperation(Operation operation) const;
    virtual bool vcsOpen(const QString &fileName);
    virtual bool vcsAdd(const QString &fileName);
    virtual bool vcsDelete(const QString &filename);

    void emitRepositoryChanged(const QString &s);
    void emitFilesChanged(const QStringList &l);

signals:
    void enabledChanged(bool);

private:
    bool m_enabled;
    CVSPlugin *m_plugin;
};

} // namespace Internal
} // namespace CVS

#endif // CVSCONTROL_H
