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

#include "processstep.h"
#include "buildstep.h"
#include "project.h"

#include <coreplugin/ifile.h>

#include <QtCore/QDebug>
#include <QtGui/QFileDialog>

using namespace ProjectExplorer;
using namespace ProjectExplorer::Internal;

ProcessStep::ProcessStep(Project *pro)
        : AbstractProcessStep(pro)
{

}

bool ProcessStep::init(const QString &buildConfigurationName)
{
    BuildConfiguration *bc = project()->buildConfiguration(buildConfigurationName);
    setEnvironment(buildConfigurationName, project()->environment(bc));
    QVariant wd = value(buildConfigurationName, "workingDirectory").toString();
    QString workingDirectory;
    if (!wd.isValid() || wd.toString().isEmpty())
        workingDirectory = "$BUILDDIR";
    else
        workingDirectory = wd.toString();
    setWorkingDirectory(buildConfigurationName, workingDirectory.replace("$BUILDDIR", project()->buildDirectory(bc)));
    return AbstractProcessStep::init(buildConfigurationName);
}

void ProcessStep::run(QFutureInterface<bool> & fi)
{
    return AbstractProcessStep::run(fi);
}

QString ProcessStep::name()
{
    return "projectexplorer.processstep";
}

void ProcessStep::setDisplayName(const QString &name)
{
    if (name.isEmpty())
        setValue("ProjectExplorer.ProcessStep.DisplayName", QVariant());
    else
        setValue("ProjectExplorer.ProcessStep.DisplayName", name);
}

QString ProcessStep::displayName()
{
    QVariant displayName = value("ProjectExplorer.ProcessStep.DisplayName");
    if (displayName.isValid())
        return displayName.toString();
    else
        return tr("Custom Process Step");
}

BuildStepConfigWidget *ProcessStep::createConfigWidget()
{
    return new ProcessStepConfigWidget(this);
}

bool ProcessStep::immutable() const
{
    return false;
}

//*******
// ProcessStepFactory
//*******

ProcessStepFactory::ProcessStepFactory()
{

}

bool ProcessStepFactory::canCreate(const QString &name) const
{
    return name == "projectexplorer.processstep";
}

BuildStep *ProcessStepFactory::create(Project *pro, const QString &name) const
{
    Q_UNUSED(name)
    return new ProcessStep(pro);
}

QStringList ProcessStepFactory::canCreateForProject(Project *pro) const
{
    Q_UNUSED(pro)
    return QStringList()<<"projectexplorer.processstep";
}
QString ProcessStepFactory::displayNameForName(const QString &name) const
{
    Q_UNUSED(name)
    return ProcessStep::tr("Custom Process Step", "item in combobox");
}

//*******
// ProcessStepConfigWidget
//*******

ProcessStepConfigWidget::ProcessStepConfigWidget(ProcessStep *step)
        : m_step(step)
{
    m_ui.setupUi(this);
    m_ui.command->setExpectedKind(Utils::PathChooser::File);
    connect(m_ui.command, SIGNAL(changed(QString)),
            this, SLOT(commandLineEditTextEdited()));
    connect(m_ui.workingDirectory, SIGNAL(changed(QString)),
            this, SLOT(workingDirectoryLineEditTextEdited()));

    connect(m_ui.nameLineEdit, SIGNAL(textEdited(const QString&)),
            this, SLOT(nameLineEditTextEdited()));
    connect(m_ui.commandArgumentsLineEdit, SIGNAL(textEdited(const QString&)),
            this, SLOT(commandArgumentsLineEditTextEdited()));
    connect(m_ui.enabledCheckBox, SIGNAL(clicked(bool)),
            this, SLOT(enabledCheckBoxClicked(bool)));
}

void ProcessStepConfigWidget::updateDetails()
{
    QString displayName = m_step->displayName();
    if (displayName.isEmpty())
        displayName = "Custom Process Step";
    m_summaryText = tr("<b>%1</b> %2 %3 %4")
                    .arg(displayName,
                         m_step->command(m_buildConfiguration),
                         m_step->arguments(m_buildConfiguration).join(" "),
                         m_step->enabled(m_buildConfiguration) ? "" : tr("(disabled)"));
    emit updateSummary();
}

QString ProcessStepConfigWidget::displayName() const
{
    return m_step->name();
}

void ProcessStepConfigWidget::init(const QString &buildConfiguration)
{
    m_buildConfiguration = buildConfiguration;
    if (buildConfiguration != QString::null) {
        m_ui.command->setPath(m_step->command(buildConfiguration));

        QString workingDirectory = m_step->value(buildConfiguration, "workingDirectory").toString();
        if (workingDirectory.isEmpty())
            workingDirectory = "$BUILDDIR";
        m_ui.workingDirectory->setPath(workingDirectory);

        m_ui.commandArgumentsLineEdit->setText(m_step->arguments(buildConfiguration).join(" "));
        m_ui.enabledCheckBox->setChecked(m_step->enabled(buildConfiguration));
    }
    m_ui.nameLineEdit->setText(m_step->displayName());
    updateDetails();
}

QString ProcessStepConfigWidget::summaryText() const
{
    return m_summaryText;
}

void ProcessStepConfigWidget::nameLineEditTextEdited()
{
    m_step->setDisplayName(m_ui.nameLineEdit->text());
    emit updateDetails();
}

void ProcessStepConfigWidget::commandLineEditTextEdited()
{
    m_step->setCommand(m_buildConfiguration, m_ui.command->path());
    updateDetails();
}

void ProcessStepConfigWidget::workingDirectoryLineEditTextEdited()
{
    QString wd = m_ui.workingDirectory->path();
    m_step->setValue(m_buildConfiguration, "workingDirectory", wd);
}

void ProcessStepConfigWidget::commandArgumentsLineEditTextEdited()
{
    m_step->setArguments(m_buildConfiguration, m_ui.commandArgumentsLineEdit->text().split(" ",
          QString::SkipEmptyParts));
    updateDetails();
}

void ProcessStepConfigWidget::enabledCheckBoxClicked(bool)
{
    m_step->setEnabled(m_buildConfiguration, m_ui.enabledCheckBox->isChecked());
    updateDetails();
}
