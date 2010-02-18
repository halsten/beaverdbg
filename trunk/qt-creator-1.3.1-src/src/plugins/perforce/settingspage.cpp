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

#include "settingspage.h"
#include "perforcesettings.h"
#include "perforceplugin.h"

#include <vcsbase/vcsbaseconstants.h>

#include <QtGui/QApplication>
#include <QtGui/QLineEdit>
#include <QtGui/QFileDialog>

using namespace Perforce::Internal;
using namespace Utils;

SettingsPageWidget::SettingsPageWidget(QWidget *parent) :
    QWidget(parent)
{
    m_ui.setupUi(this);
    m_ui.pathChooser->setPromptDialogTitle(tr("Perforce Command"));
    m_ui.pathChooser->setExpectedKind(PathChooser::Command);
    connect(m_ui.testPushButton, SIGNAL(clicked()), this, SLOT(slotTest()));
}

void SettingsPageWidget::slotTest()
{
    QString message;
    QApplication::setOverrideCursor(Qt::BusyCursor);
    setStatusText(true, tr("Testing..."));
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    const bool ok = settings().check(&message);
    QApplication::restoreOverrideCursor();
    if (ok)
        message = tr("Test succeeded.");
    setStatusText(ok, message);
}

Settings SettingsPageWidget::settings() const
{
    Settings  settings;
    settings.p4Command = m_ui.pathChooser->path();
    settings.defaultEnv = m_ui.defaultCheckBox->isChecked();
    settings.p4Port = m_ui.portLineEdit->text();
    settings.p4User = m_ui.userLineEdit->text();
    settings.p4Client=  m_ui.clientLineEdit->text();
    settings.promptToSubmit = m_ui.promptToSubmitCheckBox->isChecked();
    return settings;
}

void SettingsPageWidget::setSettings(const PerforceSettings &s)
{
    m_ui.pathChooser->setPath(s.p4Command());
    m_ui.defaultCheckBox->setChecked(s.defaultEnv());
    m_ui.portLineEdit->setText(s.p4Port());
    m_ui.clientLineEdit->setText(s.p4Client());
    m_ui.userLineEdit->setText(s.p4User());
    m_ui.promptToSubmitCheckBox->setChecked(s.promptToSubmit());
    const QString errorString = s.errorString();
    setStatusText(errorString.isEmpty(), errorString);
}

void SettingsPageWidget::setStatusText(bool ok, const QString &t)
{
    m_ui.errorLabel->setStyleSheet(ok ? QString() : QString(QLatin1String("background-color: red")));
    m_ui.errorLabel->setText(t);
}

SettingsPage::SettingsPage()
{
}

QString SettingsPage::id() const
{
    return QLatin1String("Perforce");
}

QString SettingsPage::trName() const
{
    return tr("Perforce");
}

QString SettingsPage::category() const
{
    return QLatin1String(VCSBase::Constants::VCS_SETTINGS_CATEGORY);
}

QString SettingsPage::trCategory() const
{
    return QCoreApplication::translate("VCSBase", VCSBase::Constants::VCS_SETTINGS_CATEGORY);
}

QWidget *SettingsPage::createPage(QWidget *parent)
{
    m_widget = new SettingsPageWidget(parent);
    m_widget->setSettings(PerforcePlugin::perforcePluginInstance()->settings());
    return m_widget;
}

void SettingsPage::apply()
{
    PerforcePlugin::perforcePluginInstance()->setSettings(m_widget->settings());
}
