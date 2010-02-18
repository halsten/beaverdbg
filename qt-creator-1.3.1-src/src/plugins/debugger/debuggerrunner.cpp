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

#include "debuggerrunner.h"

#include "debuggermanager.h"

#include <projectexplorer/debugginghelper.h>
#include <projectexplorer/environment.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorerconstants.h>

#include <utils/qtcassert.h>
#include <coreplugin/icore.h>

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <QtGui/QTextDocument>

namespace Debugger {
namespace Internal {

using ProjectExplorer::RunConfiguration;
using ProjectExplorer::RunControl;
using ProjectExplorer::LocalApplicationRunConfiguration;

DefaultLocalApplicationRunConfiguration::DefaultLocalApplicationRunConfiguration(const QString &executable) :
    ProjectExplorer::LocalApplicationRunConfiguration(0),
    m_executable(executable)
{
}

////////////////////////////////////////////////////////////////////////
//
// DebuggerRunControlFactory
//
////////////////////////////////////////////////////////////////////////

// A factory to create DebuggerRunControls
DebuggerRunControlFactory::DebuggerRunControlFactory(DebuggerManager *manager)
    : m_manager(manager)
{}

bool DebuggerRunControlFactory::canRun(const RunConfigurationPtr &runConfiguration, const QString &mode) const
{
    return mode == ProjectExplorer::Constants::DEBUGMODE
       && !runConfiguration.objectCast<LocalApplicationRunConfiguration>().isNull();
}

QString DebuggerRunControlFactory::displayName() const
{
    return tr("Debug");
}

RunConfigurationPtr DebuggerRunControlFactory::createDefaultRunConfiguration(const QString &executable)
{
    return RunConfigurationPtr(new DefaultLocalApplicationRunConfiguration(executable));
}

RunControl *DebuggerRunControlFactory::create(const RunConfigurationPtr &runConfiguration,
                                              const QString &mode,
                                              const DebuggerStartParametersPtr &sp)
{
    QTC_ASSERT(mode == ProjectExplorer::Constants::DEBUGMODE, return 0);
    LocalApplicationRunConfigurationPtr rc =
        runConfiguration.objectCast<LocalApplicationRunConfiguration>();
    QTC_ASSERT(!rc.isNull(), return 0);
    return new DebuggerRunControl(m_manager, sp, rc);
}

RunControl *DebuggerRunControlFactory::create(const RunConfigurationPtr &runConfiguration,
                                              const QString &mode)
{
    const DebuggerStartParametersPtr sp(new DebuggerStartParameters);
    sp->startMode = StartInternal;
    return create(runConfiguration, mode, sp);
}

QWidget *DebuggerRunControlFactory::configurationWidget(const RunConfigurationPtr &runConfiguration)
{
    // NBS TODO: Add GDB-specific configuration widget
    Q_UNUSED(runConfiguration)
    return 0;
}



////////////////////////////////////////////////////////////////////////
//
// DebuggerRunControl
//
////////////////////////////////////////////////////////////////////////


DebuggerRunControl::DebuggerRunControl(DebuggerManager *manager,
       const DebuggerStartParametersPtr &startParameters,
       QSharedPointer<LocalApplicationRunConfiguration> runConfiguration)
  : RunControl(runConfiguration),
    m_startParameters(startParameters),
    m_manager(manager),
    m_running(false)
{
    connect(m_manager, SIGNAL(debuggingFinished()),
            this, SLOT(debuggingFinished()),
            Qt::QueuedConnection);
    connect(m_manager, SIGNAL(applicationOutputAvailable(QString)),
            this, SLOT(slotAddToOutputWindowInline(QString)),
            Qt::QueuedConnection);
    connect(m_manager, SIGNAL(inferiorPidChanged(qint64)),
            this, SLOT(bringApplicationToForeground(qint64)),
            Qt::QueuedConnection);
    connect(this, SIGNAL(stopRequested()),
            m_manager, SLOT(exitDebugger()));

    if (!runConfiguration)
        return;

    // Enhance parameters by info from the project, but do not clobber
    // arguments given in the dialogs
    if (m_startParameters->executable.isEmpty())
        m_startParameters->executable = runConfiguration->executable();
    if (m_startParameters->environment.empty())
        m_startParameters->environment = runConfiguration->environment().toStringList();
    if (m_startParameters->workingDir.isEmpty())
        m_startParameters->workingDir = runConfiguration->workingDirectory();
    if (m_startParameters->startMode != StartExternal)
        m_startParameters->processArgs = runConfiguration->commandLineArguments();
    switch (m_startParameters->toolChainType) {
    case ProjectExplorer::ToolChain::UNKNOWN:
    case ProjectExplorer::ToolChain::INVALID:
        m_startParameters->toolChainType = runConfiguration->toolChainType();
        break;
    default:
        break;
    }
    if (const ProjectExplorer::Project *project = runConfiguration->project()) {
        m_startParameters->buildDir =
            project->buildDirectory(project->activeBuildConfiguration());
    }
    m_startParameters->useTerminal =
        runConfiguration->runMode() == LocalApplicationRunConfiguration::Console;
    m_startParameters->dumperLibrary =
        runConfiguration->dumperLibrary();
    m_startParameters->dumperLibraryLocations =
        runConfiguration->dumperLibraryLocations();

    QString qmakePath = ProjectExplorer::DebuggingHelperLibrary::findSystemQt(
            runConfiguration->environment());
    if (!qmakePath.isEmpty()) {
        QProcess proc;
        QStringList args;
        args.append(QLatin1String("-query"));
        args.append(QLatin1String("QT_INSTALL_HEADERS"));
        proc.start(qmakePath, args);
        proc.waitForFinished();
        QByteArray ba = proc.readAllStandardOutput().trimmed();
        QFileInfo fi(QString::fromLocal8Bit(ba) + "/..");
        m_startParameters->qtInstallPath = fi.absoluteFilePath();
    }

}

void DebuggerRunControl::start()
{
    m_running = true;
    QString errorMessage;
    QString settingsCategory;
    QString settingsPage;
    if (m_manager->checkDebugConfiguration(startParameters()->toolChainType, &errorMessage,
                                           &settingsCategory, &settingsPage)) {
        m_manager->startNewDebugger(m_startParameters);
    } else {
        error(this, errorMessage);
        emit finished();
        Core::ICore::instance()->showWarningWithOptions(tr("Debugger"), errorMessage,
                                                        QString(),
                                                        settingsCategory, settingsPage);
    }
}

void DebuggerRunControl::slotAddToOutputWindowInline(const QString &data)
{
    emit addToOutputWindowInline(this, data);
}

void DebuggerRunControl::stop()
{
    m_running = false;
    emit stopRequested();
}

void DebuggerRunControl::debuggingFinished()
{
    m_running = false;
    emit finished();
}

bool DebuggerRunControl::isRunning() const
{
    return m_running;
}

} // namespace Internal
} // namespace Debugger
