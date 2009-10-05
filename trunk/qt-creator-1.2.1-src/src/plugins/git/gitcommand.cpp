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

#include "gitcommand.h"
#include "gitconstants.h"

#include <coreplugin/icore.h>
#include <coreplugin/progressmanager/progressmanager.h>
#include <extensionsystem/pluginmanager.h>

#include <QtCore/QDebug>
#include <QtCore/QProcess>
#include <QtCore/QFuture>
#include <QtCore/QtConcurrentRun>
#include <QtCore/QFileInfo>
#include <QtCore/QCoreApplication>

namespace Git {
namespace Internal {

// Convert environment to list, default to system one.
static inline QStringList environmentToList(const ProjectExplorer::Environment &environment)
{
    const QStringList list = environment.toStringList();
    if (!list.empty())
        return list;
    return ProjectExplorer::Environment::systemEnvironment().toStringList();
}

static QString msgTermination(int exitCode, const QString &binaryPath, const QStringList &args)
{
    QString cmd = QFileInfo(binaryPath).baseName();
    if (!args.empty()) {
        cmd += QLatin1Char(' ');
        cmd += args.front();
    }
    return exitCode ?
            QCoreApplication::translate("GitCommand", "\n'%1' failed (exit code %2).\n").arg(cmd).arg(exitCode) :
            QCoreApplication::translate("GitCommand", "\n'%1' completed (exit code %2).\n").arg(cmd).arg(exitCode);
}

GitCommand::Job::Job(const QStringList &a, int t) :
    arguments(a),
    timeout(t)
{
}

GitCommand::GitCommand(const QString &binaryPath,
                       const QString &workingDirectory,
                        ProjectExplorer::Environment &environment)  :
    m_binaryPath(binaryPath),
    m_workingDirectory(workingDirectory),
    m_environment(environmentToList(environment)),
    m_reportTerminationMode(NoReport)
{
}

GitCommand::TerminationReportMode GitCommand::reportTerminationMode() const
{
    return m_reportTerminationMode;
}

void GitCommand::setTerminationReportMode(TerminationReportMode m)
{
    m_reportTerminationMode = m;
}

void GitCommand::addJob(const QStringList &arguments, int timeout)
{
    m_jobs.push_back(Job(arguments, timeout));
}

void GitCommand::execute()
{
    if (Git::Constants::debug)
        qDebug() << "GitCommand::execute" << m_workingDirectory << m_jobs.size();

    if (m_jobs.empty())
        return;

    // For some reason QtConcurrent::run() only works on this
    QFuture<void> task = QtConcurrent::run(this, &GitCommand::run);
    const QString taskName = QLatin1String("Git ") + m_jobs.front().arguments.at(0);

    Core::ICore::instance()->progressManager()->addTask(task, taskName,
                                     QLatin1String("Git.action"),
                                     Core::ProgressManager::CloseOnSuccess);
}

void GitCommand::run()
{
    if (Git::Constants::debug)
        qDebug() << "GitCommand::run" << m_workingDirectory << m_jobs.size();
    QProcess process;
    if (!m_workingDirectory.isEmpty())
        process.setWorkingDirectory(m_workingDirectory);

    process.setEnvironment(m_environment);

    QByteArray output;
    QString error;

    const int count = m_jobs.size();
    bool ok = true;
    for (int j = 0; j < count; j++) {
        if (Git::Constants::debug)
            qDebug() << "GitCommand::run" << j << '/' << count << m_jobs.at(j).arguments;

#ifdef Q_OS_WIN
        QStringList args;
        args << "/c" << m_binaryPath << m_jobs.at(j).arguments;
        process.start(QLatin1String("cmd.exe"), args);
#else
        process.start(m_binaryPath, m_jobs.at(j).arguments);
#endif
        if(!process.waitForStarted()) {
            ok = false;
            error += QString::fromLatin1("Error: \"%1\" could not be started: %2").arg(m_binaryPath, process.errorString());
            break;
        }

        process.closeWriteChannel();
        if (!process.waitForFinished(m_jobs.at(j).timeout * 1000)) {
            process.terminate();
            ok = false;
            error += QLatin1String("Error: Git timed out");
            break;
        }

        output += process.readAllStandardOutput();
        error += QString::fromLocal8Bit(process.readAllStandardError());
        switch (m_reportTerminationMode) {
        case NoReport:            
            break;
        case ReportStdout:
            output += msgTermination(process.exitCode(), m_binaryPath, m_jobs.at(j).arguments).toUtf8();
            break;
        case ReportStderr:
            error += msgTermination(process.exitCode(), m_binaryPath, m_jobs.at(j).arguments);
            break;
        }
    }

    // Special hack: Always produce output for diff
    if (ok && output.isEmpty() && m_jobs.front().arguments.at(0) == QLatin1String("diff")) {
        output += "The file does not differ from HEAD";
    } else {
        // @TODO: Remove, see below
        if (ok && m_jobs.front().arguments.at(0) == QLatin1String("status"))
            removeColorCodes(&output);
    }

    if (ok && !output.isEmpty())
        emit outputData(output);

    if (!error.isEmpty())
        emit errorText(error);

    // As it is used asynchronously, we need to delete ourselves
    this->deleteLater();
}

// Clean output from carriage return and ANSI color codes.
// @TODO: Remove once all relevant commands support "--no-color",
//("status" is  missing it as of git 1.6.2)

void GitCommand::removeColorCodes(QByteArray *data)
{
    // Remove ansi color codes that look like "ESC[<stuff>m"
    const QByteArray ansiColorEscape("\033[");
    int escapePos = 0;
    while (true) {
        const int nextEscapePos = data->indexOf(ansiColorEscape, escapePos);
        if (nextEscapePos == -1)
            break;
        const int endEscapePos = data->indexOf('m', nextEscapePos + ansiColorEscape.size());
        if (endEscapePos != -1) {
            data->remove(nextEscapePos, endEscapePos - nextEscapePos + 1);
            escapePos = nextEscapePos;
        } else {
            escapePos = nextEscapePos + ansiColorEscape.size();
        }
    }
}

} // namespace Internal
} // namespace Git
