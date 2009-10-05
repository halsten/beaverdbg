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

#include "cmakebuildenvironmentwidget.h"
#include "cmakeproject.h"
#include <projectexplorer/environmenteditmodel.h>
#include <QtGui/QVBoxLayout>

namespace {
bool debug = false;
}

using namespace CMakeProjectManager;
using namespace CMakeProjectManager::Internal;
using ProjectExplorer::EnvironmentModel;

CMakeBuildEnvironmentWidget::CMakeBuildEnvironmentWidget(CMakeProject *project)
    : BuildStepConfigWidget(), m_pro(project)
{
    QVBoxLayout *vbox = new QVBoxLayout(this);
    m_clearSystemEnvironmentCheckBox = new QCheckBox(this);
    m_clearSystemEnvironmentCheckBox->setText("Clear system environment");
    vbox->addWidget(m_clearSystemEnvironmentCheckBox);
    m_buildEnvironmentWidget = new ProjectExplorer::EnvironmentWidget(this);
    vbox->addWidget(m_buildEnvironmentWidget);

    connect(m_buildEnvironmentWidget, SIGNAL(userChangesUpdated()),
            this, SLOT(environmentModelUserChangesUpdated()));
    connect(m_clearSystemEnvironmentCheckBox, SIGNAL(toggled(bool)),
            this, SLOT(clearSystemEnvironmentCheckBoxClicked(bool)));
}

QString CMakeBuildEnvironmentWidget::displayName() const
{
    return tr("Build Environment");
}

void CMakeBuildEnvironmentWidget::init(const QString &buildConfiguration)
{
    if (debug)
        qDebug() << "Qt4BuildConfigWidget::init()";

    m_buildConfiguration = buildConfiguration;

    m_clearSystemEnvironmentCheckBox->setChecked(!m_pro->useSystemEnvironment(buildConfiguration));
    m_buildEnvironmentWidget->setBaseEnvironment(m_pro->baseEnvironment(buildConfiguration));
    m_buildEnvironmentWidget->setUserChanges(m_pro->userEnvironmentChanges(buildConfiguration));
    m_buildEnvironmentWidget->updateButtons();
}

void CMakeBuildEnvironmentWidget::environmentModelUserChangesUpdated()
{
    m_pro->setUserEnvironmentChanges(m_buildConfiguration, m_buildEnvironmentWidget->userChanges());
}

void CMakeBuildEnvironmentWidget::clearSystemEnvironmentCheckBoxClicked(bool checked)
{
    m_pro->setUseSystemEnvironment(m_buildConfiguration, !checked);
    m_buildEnvironmentWidget->setBaseEnvironment(m_pro->baseEnvironment(m_buildConfiguration));
}
