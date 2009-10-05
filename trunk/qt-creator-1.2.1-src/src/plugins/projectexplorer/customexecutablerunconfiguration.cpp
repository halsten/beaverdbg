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

#include "customexecutablerunconfiguration.h"
#include "environment.h"
#include "project.h"

#include <coreplugin/icore.h>
#include <projectexplorer/debugginghelper.h>

#include <QtGui/QCheckBox>
#include <QtGui/QFormLayout>
#include <QtGui/QLineEdit>
#include <QtGui/QLabel>
#include <QtGui/QMainWindow>
#include <QtGui/QHBoxLayout>
#include <QtGui/QToolButton>
#include <QtGui/QFileDialog>
#include <QDialogButtonBox>

using namespace ProjectExplorer;
using namespace ProjectExplorer::Internal;

class CustomDirectoryPathChooser : public Core::Utils::PathChooser
{
public:
    CustomDirectoryPathChooser(QWidget *parent)
        : Core::Utils::PathChooser(parent)
    {
    }
    virtual bool validatePath(const QString &path, QString *errorMessage = 0)
    {
        Q_UNUSED(path);
        Q_UNUSED(errorMessage);
        return true;
    }
};

CustomExecutableConfigurationWidget::CustomExecutableConfigurationWidget(CustomExecutableRunConfiguration *rc)
    : m_ignoreChange(false), m_runConfiguration(rc)
{
    QFormLayout *layout = new QFormLayout;
    layout->setMargin(0);

    m_userName = new QLineEdit(this);
    layout->addRow(tr("Name:"), m_userName);

    m_executableChooser = new Core::Utils::PathChooser(this);
    m_executableChooser->setExpectedKind(Core::Utils::PathChooser::Command);
    layout->addRow(tr("Executable:"), m_executableChooser);

    m_commandLineArgumentsLineEdit = new QLineEdit(this);
    m_commandLineArgumentsLineEdit->setMinimumWidth(200); // this shouldn't be fixed here...
    layout->addRow(tr("Arguments:"), m_commandLineArgumentsLineEdit);

    m_workingDirectory = new CustomDirectoryPathChooser(this);
    m_workingDirectory->setExpectedKind(Core::Utils::PathChooser::Directory);
    layout->addRow(tr("Working Directory:"), m_workingDirectory);

    m_useTerminalCheck = new QCheckBox(tr("Run in &Terminal"), this);
    layout->addRow(QString(), m_useTerminalCheck);

    m_environmentWidget = new EnvironmentWidget(this);
    m_environmentWidget->setBaseEnvironment(rc->baseEnvironment());
    m_environmentWidget->setUserChanges(rc->userEnvironmentChanges());

    QVBoxLayout *vbox = new QVBoxLayout(this);
    vbox->addLayout(layout);
    vbox->addWidget(m_environmentWidget);

    changed();
    
    connect(m_userName, SIGNAL(textEdited(QString)),
            this, SLOT(setUserName(QString)));
    connect(m_executableChooser, SIGNAL(changed()),
            this, SLOT(setExecutable()));
    connect(m_commandLineArgumentsLineEdit, SIGNAL(textEdited(const QString&)),
            this, SLOT(setCommandLineArguments(const QString&)));
    connect(m_workingDirectory, SIGNAL(changed()),
            this, SLOT(setWorkingDirectory()));
    connect(m_useTerminalCheck, SIGNAL(toggled(bool)),
            this, SLOT(termToggled(bool)));

    connect(m_runConfiguration, SIGNAL(changed()), this, SLOT(changed()));

    connect(m_environmentWidget, SIGNAL(userChangesUpdated()),
            this, SLOT(userChangesUpdated()));

    connect(m_runConfiguration, SIGNAL(baseEnvironmentChanged()),
            this, SLOT(baseEnvironmentChanged()));
    connect(m_runConfiguration, SIGNAL(userEnvironmentChangesChanged(QList<ProjectExplorer::EnvironmentItem>)),
            this, SLOT(userEnvironmentChangesChanged()));
}

void CustomExecutableConfigurationWidget::userChangesUpdated()
{
    m_runConfiguration->setUserEnvironmentChanges(m_environmentWidget->userChanges());
}

void CustomExecutableConfigurationWidget::baseEnvironmentChanged()
{
    m_environmentWidget->setBaseEnvironment(m_runConfiguration->baseEnvironment());
}

void CustomExecutableConfigurationWidget::userEnvironmentChangesChanged()
{
    m_environmentWidget->setUserChanges(m_runConfiguration->userEnvironmentChanges());
}


void CustomExecutableConfigurationWidget::setExecutable()
{
    m_ignoreChange = true;
    m_runConfiguration->setExecutable(m_executableChooser->path());
    m_ignoreChange = false;
}
void CustomExecutableConfigurationWidget::setCommandLineArguments(const QString &commandLineArguments)
{
    m_ignoreChange = true;
    m_runConfiguration->setCommandLineArguments(commandLineArguments);
    m_ignoreChange = false;
}
void CustomExecutableConfigurationWidget::setWorkingDirectory()
{
    m_ignoreChange = true;
    m_runConfiguration->setWorkingDirectory(m_workingDirectory->path());
    m_ignoreChange = false;
}

void CustomExecutableConfigurationWidget::setUserName(const QString &name)
{
    m_ignoreChange = true;
    m_runConfiguration->setUserName(name);
    m_ignoreChange = false;
}

void CustomExecutableConfigurationWidget::termToggled(bool on)
{
    m_ignoreChange = true;
    m_runConfiguration->setRunMode(on ? ApplicationRunConfiguration::Console
                                      : ApplicationRunConfiguration::Gui);
    m_ignoreChange = false;
}

void CustomExecutableConfigurationWidget::changed()
{
    // We triggered the change, don't update us
    if (m_ignoreChange)
        return;
    m_executableChooser->setPath(m_runConfiguration->baseExecutable());
    m_commandLineArgumentsLineEdit->setText(ProjectExplorer::Environment::joinArgumentList(m_runConfiguration->commandLineArguments()));
    m_workingDirectory->setPath(m_runConfiguration->baseWorkingDirectory());
    m_useTerminalCheck->setChecked(m_runConfiguration->runMode() == ApplicationRunConfiguration::Console);
    m_userName->setText(m_runConfiguration->userName());
}

CustomExecutableRunConfiguration::CustomExecutableRunConfiguration(Project *pro)
    : ApplicationRunConfiguration(pro),
      m_runMode(Gui),
      m_userSetName(false)
{
    m_workingDirectory = "$BUILDDIR";
    setName(tr("Custom Executable"));

    connect(pro, SIGNAL(activeBuildConfigurationChanged()),
            this, SIGNAL(baseEnvironmentChanged()));

    connect(pro, SIGNAL(environmentChanged(QString)),
            this, SIGNAL(baseEnvironmentChanged()));

}

CustomExecutableRunConfiguration::~CustomExecutableRunConfiguration()
{
}

QString CustomExecutableRunConfiguration::type() const
{
    return "ProjectExplorer.CustomExecutableRunConfiguration";
}

QString CustomExecutableRunConfiguration::baseExecutable() const
{
    return m_executable;
}

QString CustomExecutableRunConfiguration::userName() const
{
    return m_userName;
}

QString CustomExecutableRunConfiguration::executable() const
{
    QString exec;
    if (QDir::isRelativePath(m_executable)) {
        Environment env = project()->environment(project()->activeBuildConfiguration());
        exec = env.searchInPath(m_executable);
    } else {
        exec = m_executable;
    }

    if (!QFileInfo(exec).exists()) {
        // Oh the executable doesn't exists, ask the user.
        QWidget *confWidget = const_cast<CustomExecutableRunConfiguration *>(this)->configurationWidget();
        QDialog dialog(Core::ICore::instance()->mainWindow());
        dialog.setLayout(new QVBoxLayout());
        dialog.layout()->addWidget(new QLabel(tr("Could not find the executable, please specify one.")));
        dialog.layout()->addWidget(confWidget);
        QDialogButtonBox *dbb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(dbb, SIGNAL(accepted()), &dialog, SLOT(accept()));
        connect(dbb, SIGNAL(rejected()), &dialog, SLOT(reject()));
        dialog.layout()->addWidget(dbb);

        QString oldExecutable = m_executable;
        QString oldWorkingDirectory = m_workingDirectory;
        QStringList oldCmdArguments = m_cmdArguments;
        
        if (dialog.exec()) {
            return executable();
        } else {
            CustomExecutableRunConfiguration *that = const_cast<CustomExecutableRunConfiguration *>(this);
            that->m_executable = oldExecutable;
            that->m_workingDirectory = oldWorkingDirectory;
            that->m_cmdArguments = oldCmdArguments;
            emit that->changed();
            return QString::null;
        }
    }
    return exec;
}

ApplicationRunConfiguration::RunMode CustomExecutableRunConfiguration::runMode() const
{
    return m_runMode;
}

QString CustomExecutableRunConfiguration::baseWorkingDirectory() const
{
    return m_workingDirectory;
}

QString CustomExecutableRunConfiguration::workingDirectory() const
{
    QString wd = m_workingDirectory;
    QString bd = project()->buildDirectory(project()->activeBuildConfiguration());
    return wd.replace("$BUILDDIR", QDir::cleanPath(bd));
}

QStringList CustomExecutableRunConfiguration::commandLineArguments() const
{
    return m_cmdArguments;
}

ProjectExplorer::Environment CustomExecutableRunConfiguration::baseEnvironment() const
{
    // TODO use either System Environment
    // build environment
    // or empty
    //Environment env = Environment(QProcess::systemEnvironment());

    QString config = project()->activeBuildConfiguration();
    ProjectExplorer::Environment env = project()->environment(project()->activeBuildConfiguration());
    return env;
}

ProjectExplorer::Environment CustomExecutableRunConfiguration::environment() const
{
    ProjectExplorer::Environment env = baseEnvironment();
    env.modify(userEnvironmentChanges());
    return env;
}

QList<ProjectExplorer::EnvironmentItem> CustomExecutableRunConfiguration::userEnvironmentChanges() const
{
    return m_userEnvironmentChanges;
}

void CustomExecutableRunConfiguration::setUserEnvironmentChanges(const QList<ProjectExplorer::EnvironmentItem> &diff)
{
    if (m_userEnvironmentChanges != diff) {
        m_userEnvironmentChanges = diff;
        emit userEnvironmentChangesChanged(diff);
    }
}

void CustomExecutableRunConfiguration::save(PersistentSettingsWriter &writer) const
{
    writer.saveValue("Executable", m_executable);
    writer.saveValue("Arguments", m_cmdArguments);
    writer.saveValue("WorkingDirectory", m_workingDirectory);
    writer.saveValue("UseTerminal", m_runMode == Console);
    writer.saveValue("UserSetName", m_userSetName);
    writer.saveValue("UserName", m_userName);
    writer.saveValue("UserEnvironmentChanges", ProjectExplorer::EnvironmentItem::toStringList(m_userEnvironmentChanges));
    ApplicationRunConfiguration::save(writer);
}

void CustomExecutableRunConfiguration::restore(const PersistentSettingsReader &reader)
{
    m_executable = reader.restoreValue("Executable").toString();
    m_cmdArguments = reader.restoreValue("Arguments").toStringList();
    m_workingDirectory = reader.restoreValue("WorkingDirectory").toString();
    m_runMode = reader.restoreValue("UseTerminal").toBool() ? Console : Gui;
    m_userSetName = reader.restoreValue("UserSetName").toBool();
    m_userName = reader.restoreValue("UserName").toString();
    m_userEnvironmentChanges = ProjectExplorer::EnvironmentItem::fromStringList(reader.restoreValue("UserEnvironmentChanges").toStringList());
    ApplicationRunConfiguration::restore(reader);
}

void CustomExecutableRunConfiguration::setExecutable(const QString &executable)
{
    m_executable = executable;
    if (!m_userSetName)
        setName(tr("Run %1").arg(m_executable));
    emit changed();
}

void CustomExecutableRunConfiguration::setCommandLineArguments(const QString &commandLineArguments)
{
    m_cmdArguments = ProjectExplorer::Environment::parseCombinedArgString(commandLineArguments);
    emit changed();
}

void CustomExecutableRunConfiguration::setWorkingDirectory(const QString &workingDirectory)
{
    m_workingDirectory = workingDirectory;
    emit changed();
}

void CustomExecutableRunConfiguration::setRunMode(RunMode runMode)
{
    m_runMode = runMode;
    emit changed();
}

QWidget *CustomExecutableRunConfiguration::configurationWidget()
{
    return new CustomExecutableConfigurationWidget(this);
}

void CustomExecutableRunConfiguration::setUserName(const QString &name)
{
    if (name.isEmpty()) {
        m_userName = name;
        m_userSetName = false;
        setName(tr("Run %1").arg(m_executable));
    } else {
        m_userName = name;
        m_userSetName = true;
        setName(name);
    }
    emit changed();
}

QString CustomExecutableRunConfiguration::dumperLibrary() const
{
    QString qmakePath = ProjectExplorer::DebuggingHelperLibrary::findSystemQt(environment());
    return ProjectExplorer::DebuggingHelperLibrary::debuggingHelperLibrary(qmakePath);
}


// Factory

CustomExecutableRunConfigurationFactory::CustomExecutableRunConfigurationFactory()
{
}

CustomExecutableRunConfigurationFactory::~CustomExecutableRunConfigurationFactory()
{

}

// used to recreate the runConfigurations when restoring settings
bool CustomExecutableRunConfigurationFactory::canCreate(const QString &type) const
{
    return type == "ProjectExplorer.CustomExecutableRunConfiguration";
}

QSharedPointer<RunConfiguration> CustomExecutableRunConfigurationFactory::create(Project *project, const QString &type)
{
    if (type == "ProjectExplorer.CustomExecutableRunConfiguration") {
        QSharedPointer<RunConfiguration> rc(new CustomExecutableRunConfiguration(project));
        rc->setName(tr("Custom Executable"));
        return rc;
    } else {
        return QSharedPointer<RunConfiguration>(0);
    }
}

QStringList CustomExecutableRunConfigurationFactory::canCreate(Project *pro) const
{
    Q_UNUSED(pro)
    return QStringList()<< "ProjectExplorer.CustomExecutableRunConfiguration";
}

QString CustomExecutableRunConfigurationFactory::nameForType(const QString &type) const
{
    if (type == "ProjectExplorer.CustomExecutableRunConfiguration")
        return tr("Custom Executable");
    else
        return QString();
}
