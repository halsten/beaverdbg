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

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include "ui_settingsdialog.h"

#include <QtCore/QList>

#include "coreplugin/dialogs/ioptionspage.h"

namespace Core {
namespace Internal {

class SettingsDialog : public QDialog, public ::Ui::SettingsDialog
{
    Q_OBJECT

public:
    SettingsDialog(QWidget *parent,
                   const QString &initialCategory = QString(),
                   const QString &initialPage = QString());
    ~SettingsDialog();

    // Run the dialog and return true if 'Ok' was choosen or 'Apply' was invoked
    // at least once
    bool execDialog();

public slots:
    void done(int);

private slots:
    void pageSelected();
    void accept();
    void reject();
    void apply();

private:
    QList<Core::IOptionsPage*> m_pages;
    bool m_applied;
    QString m_currentCategory;
    QString m_currentPage;
};

} // namespace Internal
} // namespace Core

#endif // SETTINGSDIALOG_H
