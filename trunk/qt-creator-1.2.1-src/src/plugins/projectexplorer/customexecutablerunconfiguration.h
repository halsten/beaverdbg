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

#ifndef CUSTOMEXECUTABLERUNCONFIGURATION_H
#define CUSTOMEXECUTABLERUNCONFIGURATION_H

#include "applicationrunconfiguration.h"

#include <utils/pathchooser.h>
#include <projectexplorer/environmenteditmodel.h>

#include <QtGui/QToolButton>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QLineEdit;
QT_END_NAMESPACE

namespace ProjectExplorer {
    
namespace Internal {
    class CustomExecutableConfigurationWidget;
}

class PROJECTEXPLORER_EXPORT CustomExecutableRunConfiguration : public ApplicationRunConfiguration
{
    // the configuration widget needs to setExecutable setWorkingDirectory and setCommandLineArguments
    friend class Internal::CustomExecutableConfigurationWidget;    
    Q_OBJECT

public:
    CustomExecutableRunConfiguration(Project *pro);
    ~CustomExecutableRunConfiguration();
    virtual QString type() const;

    /**
     * Returns the executable, looks in the environment for it and might even
     * ask the user if none is specified
     */
    virtual QString executable() const;

    /**
     * Returns only what is stored in the internal variable, not what we might
     * get after extending it with a path or asking the user. This value is
     * needed for the configuration widget.
     */
    QString baseExecutable() const;

    /**
     * Returns the name the user has set, if he has set one
     */
    QString userName() const;

    virtual ApplicationRunConfiguration::RunMode runMode() const;
    virtual QString workingDirectory() const;
    QString baseWorkingDirectory() const;
    virtual QStringList commandLineArguments() const;
    virtual Environment environment() const;

    virtual void save(PersistentSettingsWriter &writer) const;
    virtual void restore(const PersistentSettingsReader &reader);

    virtual QWidget *configurationWidget();
    virtual QString dumperLibrary() const;

signals:
    void changed();

    void baseEnvironmentChanged();
    void userEnvironmentChangesChanged(const QList<ProjectExplorer::EnvironmentItem> &diff);


private:
    ProjectExplorer::Environment baseEnvironment() const;
    void setUserEnvironmentChanges(const QList<ProjectExplorer::EnvironmentItem> &diff);
    QList<ProjectExplorer::EnvironmentItem> userEnvironmentChanges() const;

    void setExecutable(const QString &executable);
    void setCommandLineArguments(const QString &commandLineArguments);
    void setWorkingDirectory(const QString &workingDirectory);
    void setUserName(const QString &name);
    void setRunMode(RunMode runMode);
    QString m_executable;
    QString m_workingDirectory;
    QStringList m_cmdArguments;
    RunMode m_runMode;
    bool m_userSetName;
    QString m_userName;
    QList<ProjectExplorer::EnvironmentItem> m_userEnvironmentChanges;
};

class CustomExecutableRunConfigurationFactory : public IRunConfigurationFactory
{
    Q_OBJECT

public:
    CustomExecutableRunConfigurationFactory();
    virtual ~CustomExecutableRunConfigurationFactory();
    // used to recreate the runConfigurations when restoring settings
    virtual bool canCreate(const QString &type) const;
    virtual QSharedPointer<RunConfiguration> create(Project *project, const QString &type);
    QStringList canCreate(Project *pro) const;
    QString nameForType(const QString &type) const;
};

namespace Internal {

class CustomExecutableConfigurationWidget : public QWidget
{
    Q_OBJECT

public:
    CustomExecutableConfigurationWidget(CustomExecutableRunConfiguration *rc);

private slots:
    void changed();

    void setExecutable();
    void setCommandLineArguments(const QString &commandLineArguments);
    void setUserName(const QString &name);
    void setWorkingDirectory();
    void termToggled(bool);

    void userChangesUpdated();
    void baseEnvironmentChanged();
    void userEnvironmentChangesChanged();

private:
    bool m_ignoreChange;
    CustomExecutableRunConfiguration *m_runConfiguration;
    Core::Utils::PathChooser *m_executableChooser;
    QLineEdit *m_userName;
    QLineEdit *m_commandLineArgumentsLineEdit;
    Core::Utils::PathChooser *m_workingDirectory;
    QCheckBox *m_useTerminalCheck;
    ProjectExplorer::EnvironmentWidget *m_environmentWidget;
};

} // namespace Internal
} // namespace ProjectExplorer

#endif // CUSTOMEXECUTABLERUNCONFIGURATION_H
