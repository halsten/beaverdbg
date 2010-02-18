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

#include "qt4buildenvironmentwidget.h"
#include "qt4project.h"

#include <projectexplorer/environmenteditmodel.h>

#include <QtGui/QCheckBox>

namespace {
bool debug = false;
}

using namespace Qt4ProjectManager;
using namespace Qt4ProjectManager::Internal;

Qt4BuildEnvironmentWidget::Qt4BuildEnvironmentWidget(Qt4Project *project)
    : BuildConfigWidget(), m_pro(project)
{
    QVBoxLayout *vbox = new QVBoxLayout(this);
    vbox->setMargin(0);

    m_clearSystemEnvironmentCheckBox = new QCheckBox(this);
    m_clearSystemEnvironmentCheckBox->setText(tr("Clear system environment"));

    m_buildEnvironmentWidget = new ProjectExplorer::EnvironmentWidget(this, m_clearSystemEnvironmentCheckBox);
    vbox->addWidget(m_buildEnvironmentWidget);

    connect(m_buildEnvironmentWidget, SIGNAL(userChangesUpdated()),
            this, SLOT(environmentModelUserChangesUpdated()));

    connect(m_clearSystemEnvironmentCheckBox, SIGNAL(toggled(bool)),
            this, SLOT(clearSystemEnvironmentCheckBoxClicked(bool)));
}

QString Qt4BuildEnvironmentWidget::displayName() const
{
    return tr("Build Environment");
}

void Qt4BuildEnvironmentWidget::init(const QString &buildConfiguration)
{
    if (debug)
        qDebug() << "Qt4BuildConfigWidget::init()";

    m_buildConfiguration = buildConfiguration;
    ProjectExplorer::BuildConfiguration *bc = m_pro->buildConfiguration(buildConfiguration);
    m_clearSystemEnvironmentCheckBox->setChecked(!m_pro->useSystemEnvironment(bc));
    m_buildEnvironmentWidget->setBaseEnvironment(m_pro->baseEnvironment(bc));
    m_buildEnvironmentWidget->setUserChanges(m_pro->userEnvironmentChanges(bc));
    m_buildEnvironmentWidget->updateButtons();
}

void Qt4BuildEnvironmentWidget::environmentModelUserChangesUpdated()
{
    m_pro->setUserEnvironmentChanges(m_pro->buildConfiguration(m_buildConfiguration),
                                     m_buildEnvironmentWidget->userChanges());
}

void Qt4BuildEnvironmentWidget::clearSystemEnvironmentCheckBoxClicked(bool checked)
{
    ProjectExplorer::BuildConfiguration *bc = m_pro->buildConfiguration(m_buildConfiguration);
    m_pro->setUseSystemEnvironment(bc, !checked);
    m_buildEnvironmentWidget->setBaseEnvironment(m_pro->baseEnvironment(bc));
}
