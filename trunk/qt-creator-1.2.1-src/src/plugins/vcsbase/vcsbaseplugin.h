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

#ifndef VCSBASEPLUGIN_H
#define VCSBASEPLUGIN_H

#include <extensionsystem/iplugin.h>

#include <QtCore/QObject>

QT_BEGIN_NAMESPACE
class QStandardItemModel;
QT_END_NAMESPACE

namespace VCSBase {
namespace Internal {

struct VCSBaseSettings;
class VCSBaseSettingsPage;

class VCSBasePlugin : public ExtensionSystem::IPlugin
{
    Q_OBJECT

public:
    VCSBasePlugin();
    ~VCSBasePlugin();

    bool initialize(const QStringList &arguments, QString *error_message);

    void extensionsInitialized();

    static VCSBasePlugin *instance();

    VCSBaseSettings settings() const;

    // Model of user nick names used for the submit
    // editor. Stored centrally here to achieve delayed
    // initialization and updating on settings change.
    QStandardItemModel *nickNameModel();

signals:
    void settingsChanged(const VCSBase::Internal::VCSBaseSettings& s);

private slots:
    void slotSettingsChanged();

private:
    void populateNickNameModel();

    static VCSBasePlugin *m_instance;
    VCSBaseSettingsPage *m_settingsPage;
    QStandardItemModel *m_nickNameModel;
};

} // namespace Internal
} // namespace VCSBase

#endif // VCSBASEPLUGIN_H
