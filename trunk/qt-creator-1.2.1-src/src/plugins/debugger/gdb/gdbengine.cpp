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

#define QT_NO_CAST_FROM_ASCII

#include "gdbengine.h"
#include "gdboptionspage.h"

#include "watchutils.h"
#include "debuggeractions.h"
#include "debuggerconstants.h"
#include "debuggermanager.h"
#include "gdbmi.h"
#include "procinterrupt.h"

#include "disassemblerhandler.h"
#include "breakhandler.h"
#include "moduleshandler.h"
#include "registerhandler.h"
#include "stackhandler.h"
#include "watchhandler.h"
#include "sourcefileswindow.h"

#include "debuggerdialogs.h"

#include <utils/qtcassert.h>
#include <texteditor/itexteditor.h>
#include <coreplugin/icore.h>

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>

#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QLabel>
#include <QtGui/QMainWindow>
#include <QtGui/QMessageBox>
#include <QtGui/QToolTip>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QPushButton>
#ifdef Q_OS_WIN
#    include "shared/sharedlibraryinjector.h"
#endif

#ifdef Q_OS_UNIX
#include <unistd.h>
#include <dlfcn.h>
#endif
#include <ctype.h>

using namespace Debugger;
using namespace Debugger::Internal;
using namespace Debugger::Constants;

Q_DECLARE_METATYPE(Debugger::Internal::GdbMi);

//#define DEBUG_PENDING  1
//#define DEBUG_SUBITEM  1

#if DEBUG_PENDING
#   define PENDING_DEBUG(s) qDebug() << s
#else
#   define PENDING_DEBUG(s)
#endif

#define STRINGIFY_INTERNAL(x) #x
#define STRINGIFY(x) STRINGIFY_INTERNAL(x)
#define CB(callback) &GdbEngine::callback, STRINGIFY(callback)

static int &currentToken()
{
    static int token = 0;
    return token;
}

static const QString tooltipIName = _("tooltip");

///////////////////////////////////////////////////////////////////////
//
// GdbEngine
//
///////////////////////////////////////////////////////////////////////

GdbEngine::GdbEngine(DebuggerManager *parent) :
#ifdef Q_OS_WIN // Do injection loading with MinGW (call loading does not work with 64bit)
    m_dumperInjectionLoad(true),
#else
    m_dumperInjectionLoad(false),
#endif
    q(parent),
    qq(parent->engineInterface())
{
    m_stubProc.setMode(Core::Utils::ConsoleProcess::Debug);
#ifdef Q_OS_UNIX
    m_stubProc.setSettings(Core::ICore::instance()->settings());
#endif
    initializeVariables();
    initializeConnections();
}

GdbEngine::~GdbEngine()
{
    // prevent sending error messages afterwards
    m_gdbProc.disconnect(this);
}

void GdbEngine::initializeConnections()
{
    // Gdb Process interaction
    connect(&m_gdbProc, SIGNAL(error(QProcess::ProcessError)),
        this, SLOT(gdbProcError(QProcess::ProcessError)));
    connect(&m_gdbProc, SIGNAL(readyReadStandardOutput()),
        this, SLOT(readGdbStandardOutput()));
    connect(&m_gdbProc, SIGNAL(readyReadStandardError()),
        this, SLOT(readGdbStandardError()));
    connect(&m_gdbProc, SIGNAL(finished(int, QProcess::ExitStatus)),
        q, SLOT(exitDebugger()));

    connect(&m_stubProc, SIGNAL(processError(QString)),
        this, SLOT(stubError(QString)));
    connect(&m_stubProc, SIGNAL(processStarted()),
        this, SLOT(stubStarted()));
    connect(&m_stubProc, SIGNAL(wrapperStopped()),
        q, SLOT(exitDebugger()));

    connect(&m_uploadProc, SIGNAL(error(QProcess::ProcessError)),
        this, SLOT(uploadProcError(QProcess::ProcessError)));
    connect(&m_uploadProc, SIGNAL(readyReadStandardOutput()),
        this, SLOT(readUploadStandardOutput()));
    connect(&m_uploadProc, SIGNAL(readyReadStandardError()),
        this, SLOT(readUploadStandardError()));

    // Output
    connect(&m_outputCollector, SIGNAL(byteDelivery(QByteArray)),
        this, SLOT(readDebugeeOutput(QByteArray)));

    connect(this, SIGNAL(gdbOutputAvailable(QString,QString)),
        q, SLOT(showDebuggerOutput(QString,QString)),
        Qt::QueuedConnection);
    connect(this, SIGNAL(gdbInputAvailable(QString,QString)),
        q, SLOT(showDebuggerInput(QString,QString)),
        Qt::QueuedConnection);
    connect(this, SIGNAL(applicationOutputAvailable(QString)),
        q, SLOT(showApplicationOutput(QString)),
        Qt::QueuedConnection);

    // FIXME: These trigger even if the engine is not active
    connect(theDebuggerAction(UseDebuggingHelpers), SIGNAL(valueChanged(QVariant)),
        this, SLOT(setUseDebuggingHelpers(QVariant)));
    connect(theDebuggerAction(DebugDebuggingHelpers), SIGNAL(valueChanged(QVariant)),
        this, SLOT(setDebugDebuggingHelpers(QVariant)));
    connect(theDebuggerAction(RecheckDebuggingHelpers), SIGNAL(triggered()),
        this, SLOT(recheckDebuggingHelperAvailability()));
}

void GdbEngine::initializeVariables()
{
    m_debuggingHelperState = DebuggingHelperUninitialized;
    m_gdbVersion = 100;
    m_gdbBuildVersion = -1;

    m_fullToShortName.clear();
    m_shortToFullName.clear();
    m_varToType.clear();

    m_modulesListOutdated = true;
    m_oldestAcceptableToken = -1;
    m_outputCodec = QTextCodec::codecForLocale();
    m_pendingRequests = 0;
    m_autoContinue = false;
    m_waitingForFirstBreakpointToBeHit = false;
    m_commandsToRunOnTemporaryBreak.clear();
    m_cookieForToken.clear();
    m_customOutputForToken.clear();

    m_pendingConsoleStreamOutput.clear();
    m_pendingTargetStreamOutput.clear();
    m_pendingLogStreamOutput.clear();

    m_inbuffer.clear();

    m_address.clear();
    m_currentFunctionArgs.clear();
    m_currentFrame.clear();
    m_dumperHelper = QtDumperHelper();

    // FIXME: unhandled:
    //m_outputCodecState = QTextCodec::ConverterState();
    //OutputCollector m_outputCollector;
    //QProcess m_gdbProc;
    //QProcess m_uploadProc;
    //Core::Utils::ConsoleProcess m_stubProc;
}

void GdbEngine::gdbProcError(QProcess::ProcessError error)
{
    QString msg;
    bool kill = true;
    switch (error) {
        case QProcess::FailedToStart:
            kill = false;
            msg = tr("The Gdb process failed to start. Either the "
                "invoked program '%1' is missing, or you may have insufficient "
                "permissions to invoke the program.")
                .arg(theDebuggerStringSetting(GdbLocation));
            break;
        case QProcess::Crashed:
            kill = false;
            msg = tr("The Gdb process crashed some time after starting "
                "successfully.");
            break;
        case QProcess::Timedout:
            msg = tr("The last waitFor...() function timed out. "
                "The state of QProcess is unchanged, and you can try calling "
                "waitFor...() again.");
            break;
        case QProcess::WriteError:
            msg = tr("An error occurred when attempting to write "
                "to the Gdb process. For example, the process may not be running, "
                "or it may have closed its input channel.");
            break;
        case QProcess::ReadError:
            msg = tr("An error occurred when attempting to read from "
                "the Gdb process. For example, the process may not be running.");
            break;
        default:
            msg = tr("An unknown error in the Gdb process occurred. "
                "This is the default return value of error().");
    }

    q->showStatusMessage(msg);
    QMessageBox::critical(q->mainWindow(), tr("Error"), msg);
    // act as if it was closed by the core
    if (kill)
        q->exitDebugger();
}

void GdbEngine::uploadProcError(QProcess::ProcessError error)
{
    QString msg;
    switch (error) {
        case QProcess::FailedToStart:
            msg = tr("The upload process failed to start. Either the "
                "invoked script '%1' is missing, or you may have insufficient "
                "permissions to invoke the program.")
                .arg(theDebuggerStringSetting(GdbLocation));
            break;
        case QProcess::Crashed:
            msg = tr("The upload process crashed some time after starting "
                "successfully.");
            break;
        case QProcess::Timedout:
            msg = tr("The last waitFor...() function timed out. "
                "The state of QProcess is unchanged, and you can try calling "
                "waitFor...() again.");
            break;
        case QProcess::WriteError:
            msg = tr("An error occurred when attempting to write "
                "to the upload process. For example, the process may not be running, "
                "or it may have closed its input channel.");
            break;
        case QProcess::ReadError:
            msg = tr("An error occurred when attempting to read from "
                "the upload process. For example, the process may not be running.");
            break;
        default:
            msg = tr("An unknown error in the upload process occurred. "
                "This is the default return value of error().");
    }

    q->showStatusMessage(msg);
    QMessageBox::critical(q->mainWindow(), tr("Error"), msg);
}

void GdbEngine::readUploadStandardOutput()
{
    QByteArray ba = m_uploadProc.readAllStandardOutput();
    gdbOutputAvailable(_("upload-out:"), QString::fromLocal8Bit(ba, ba.length()));
}

void GdbEngine::readUploadStandardError()
{
    QByteArray ba = m_uploadProc.readAllStandardError();
    gdbOutputAvailable(_("upload-err:"), QString::fromLocal8Bit(ba, ba.length()));
}

#if 0
static void dump(const char *first, const char *middle, const QString & to)
{
    QByteArray ba(first, middle - first);
    Q_UNUSED(to);
    // note that qDebug cuts off output after a certain size... (bug?)
    qDebug("\n>>>>> %s\n%s\n====\n%s\n<<<<<\n",
        qPrintable(currentTime()),
        qPrintable(QString(ba).trimmed()),
        qPrintable(to.trimmed()));
    //qDebug() << "";
    //qDebug() << qPrintable(currentTime())
    //    << " Reading response:  " << QString(ba).trimmed() << "\n";
}
#endif

void GdbEngine::readDebugeeOutput(const QByteArray &data)
{
    emit applicationOutputAvailable(m_outputCodec->toUnicode(
            data.constData(), data.length(), &m_outputCodecState));
}

void GdbEngine::debugMessage(const QString &msg)
{
    emit gdbOutputAvailable(_("debug:"), msg);
}

void GdbEngine::handleResponse(const QByteArray &buff)
{
    static QTime lastTime;

    emit gdbOutputAvailable(_("            "), currentTime());
    emit gdbOutputAvailable(_("stdout:"), QString::fromLocal8Bit(buff, buff.length()));

#if 0
    qDebug() // << "#### start response handling #### "
        << currentTime()
        << lastTime.msecsTo(QTime::currentTime()) << "ms,"
        << "buf:" << buff.left(1500) << "..."
        //<< "buf:" << buff
        << "size:" << buff.size();
#else
    //qDebug() << "buf:" << buff;
#endif

    lastTime = QTime::currentTime();

    if (buff.isEmpty() || buff == "(gdb) ")
        return;

    const char *from = buff.constData();
    const char *to = from + buff.size();
    const char *inner;

    int token = -1;
    // token is a sequence of numbers
    for (inner = from; inner != to; ++inner)
        if (*inner < '0' || *inner > '9')
            break;
    if (from != inner) {
        token = QByteArray(from, inner - from).toInt();
        from = inner;
        //qDebug() << "found token" << token;
    }

    // next char decides kind of record
    const char c = *from++;
    //qDebug() << "CODE:" << c;
    switch (c) {
        case '*':
        case '+':
        case '=': {
            QByteArray asyncClass;
            for (; from != to; ++from) {
                const char c = *from;
                if (!isNameChar(c))
                    break;
                asyncClass += *from;
            }
            //qDebug() << "ASYNCCLASS" << asyncClass;

            GdbMi record;
            while (from != to) {
                GdbMi data;
                if (*from != ',') {
                    // happens on archer where we get 
                    // 23^running <NL> *running,thread-id="all" <NL> (gdb) 
                    record.m_type = GdbMi::Tuple;
                    break;
                }
                ++from; // skip ','
                data.parseResultOrValue(from, to);
                if (data.isValid()) {
                    //qDebug() << "parsed response:" << data.toString();
                    record.m_children += data;
                    record.m_type = GdbMi::Tuple;
                }
            }
            if (asyncClass == "stopped") {
                handleAsyncOutput(record);
            } else if (asyncClass == "running") {
                // Archer has 'thread-id="all"' here
            } else if (asyncClass == "library-loaded") {
                // Archer has 'id="/usr/lib/libdrm.so.2",
                // target-name="/usr/lib/libdrm.so.2",
                // host-name="/usr/lib/libdrm.so.2",
                // symbols-loaded="0"
                QByteArray id = record.findChild("id").data();
                if (!id.isEmpty())
                    q->showStatusMessage(tr("Library %1 loaded.").arg(_(id)));
            } else if (asyncClass == "library-unloaded") {
                // Archer has 'id="/usr/lib/libdrm.so.2",
                // target-name="/usr/lib/libdrm.so.2",
                // host-name="/usr/lib/libdrm.so.2"
                QByteArray id = record.findChild("id").data();
                q->showStatusMessage(tr("Library %1 unloaded.").arg(_(id)));
            } else if (asyncClass == "thread-group-created") {
                // Archer has "{id="28902"}" 
                QByteArray id = record.findChild("id").data();
                q->showStatusMessage(tr("Thread group %1 created.").arg(_(id)));
            } else if (asyncClass == "thread-created") {
                //"{id="1",group-id="28902"}" 
                QByteArray id = record.findChild("id").data();
                q->showStatusMessage(tr("Thread %1 created.").arg(_(id)));
            } else if (asyncClass == "thread-group-exited") {
                // Archer has "{id="28902"}" 
                QByteArray id = record.findChild("id").data();
                q->showStatusMessage(tr("Thread group %1 exited.").arg(_(id)));
            } else if (asyncClass == "thread-exited") {
                //"{id="1",group-id="28902"}" 
                QByteArray id = record.findChild("id").data();
                QByteArray groupid = record.findChild("group-id").data();
                q->showStatusMessage(tr("Thread %1 in group %2 exited.")
                    .arg(_(id)).arg(_(groupid)));
            } else if (asyncClass == "thread-selected") {
                QByteArray id = record.findChild("id").data();
                q->showStatusMessage(tr("Thread %1 selected.").arg(_(id)));
                //"{id="2"}" 
            #if defined(Q_OS_MAC)
            } else if (asyncClass == "shlibs-updated") {
                // MAC announces updated libs
            } else if (asyncClass == "shlibs-added") {
                // MAC announces added libs
                // {shlib-info={num="2", name="libmathCommon.A_debug.dylib",
                // kind="-", dyld-addr="0x7f000", reason="dyld", requested-state="Y",
                // state="Y", path="/usr/lib/system/libmathCommon.A_debug.dylib",
                // description="/usr/lib/system/libmathCommon.A_debug.dylib",
                // loaded_addr="0x7f000", slide="0x7f000", prefix=""}}
            #endif
            } else {
                qDebug() << "IGNORED ASYNC OUTPUT"
                    << asyncClass << record.toString();
            }
            break;
        }

        case '~': {
            QByteArray data = GdbMi::parseCString(from, to);
            m_pendingConsoleStreamOutput += data;
            if (data.startsWith("Reading symbols from ")) {
                q->showStatusMessage(tr("Reading %1...").arg(_(data.mid(21))));
            }
            break;
        }

        case '@': {
            QByteArray data = GdbMi::parseCString(from, to);
            m_pendingTargetStreamOutput += data;
            break;
        }

        case '&': {
            QByteArray data = GdbMi::parseCString(from, to);
            m_pendingLogStreamOutput += data;
            // On Windows, the contents seem to depend on the debugger
            // version and/or OS version used.
            if (data.startsWith("warning:"))
                qq->showApplicationOutput(_(data.mid(9))); // cut "warning: "
            break;
        }

        case '^': {
            GdbResultRecord record;

            record.token = token;

            for (inner = from; inner != to; ++inner)
                if (*inner < 'a' || *inner > 'z')
                    break;

            QByteArray resultClass = QByteArray::fromRawData(from, inner - from);
            if (resultClass == "done")
                record.resultClass = GdbResultDone;
            else if (resultClass == "running")
                record.resultClass = GdbResultRunning;
            else if (resultClass == "connected")
                record.resultClass = GdbResultConnected;
            else if (resultClass == "error")
                record.resultClass = GdbResultError;
            else if (resultClass == "exit")
                record.resultClass = GdbResultExit;
            else
                record.resultClass = GdbResultUnknown;

            from = inner;
            if (from != to) {
                if (*from == ',') {
                    ++from;
                    record.data.parseTuple_helper(from, to);
                    record.data.m_type = GdbMi::Tuple;
                    record.data.m_name = "data";
                } else {
                    // Archer has this
                    record.data.m_type = GdbMi::Tuple;
                    record.data.m_name = "data";
                }
            }

            //qDebug() << "\nLOG STREAM:" + m_pendingLogStreamOutput;
            //qDebug() << "\nTARGET STREAM:" + m_pendingTargetStreamOutput;
            //qDebug() << "\nCONSOLE STREAM:" + m_pendingConsoleStreamOutput;
            record.data.setStreamOutput("logstreamoutput",
                m_pendingLogStreamOutput);
            record.data.setStreamOutput("targetstreamoutput",
                m_pendingTargetStreamOutput);
            record.data.setStreamOutput("consolestreamoutput",
                m_pendingConsoleStreamOutput);
            QByteArray custom = m_customOutputForToken[token];
            if (!custom.isEmpty())
                record.data.setStreamOutput("customvaluecontents",
                    '{' + custom + '}');
            //m_customOutputForToken.remove(token);
            m_pendingLogStreamOutput.clear();
            m_pendingTargetStreamOutput.clear();
            m_pendingConsoleStreamOutput.clear();

            handleResultRecord(record);
            break;
        }
        default: {
            qDebug() << "UNKNOWN RESPONSE TYPE" << c;
            break;
        }
    }
}

void GdbEngine::handleStubAttached(const GdbResultRecord &, const QVariant &)
{
    qq->notifyInferiorStopped();
    handleAqcuiredInferior();
    m_autoContinue = true;
}

void GdbEngine::stubStarted()
{
    const qint64 attachedPID = m_stubProc.applicationPID();
    qq->notifyInferiorPidChanged(attachedPID);
    postCommand(_("attach %1").arg(attachedPID), CB(handleStubAttached));
}

void GdbEngine::stubError(const QString &msg)
{
    QMessageBox::critical(q->mainWindow(), tr("Debugger Error"), msg);
}

void GdbEngine::readGdbStandardError()
{
    qWarning() << "Unexpected gdb stderr:" << m_gdbProc.readAllStandardError();
}

void GdbEngine::readGdbStandardOutput()
{
    int newstart = 0;
    int scan = m_inbuffer.size();

    m_inbuffer.append(m_gdbProc.readAllStandardOutput());

    while (newstart < m_inbuffer.size()) {
        int start = newstart;
        int end = m_inbuffer.indexOf('\n', scan);
        if (end < 0) {
            m_inbuffer.remove(0, start);
            return;
        }
        newstart = end + 1;
        scan = newstart;
        if (end == start)
            continue;
        #if defined(Q_OS_WIN)
        if (m_inbuffer.at(end - 1) == '\r') {
            --end;
            if (end == start)
                continue;
        }
        #endif
        handleResponse(QByteArray::fromRawData(m_inbuffer.constData() + start, end - start));
    }
    m_inbuffer.clear();
}

void GdbEngine::interruptInferior()
{
    qq->notifyInferiorStopRequested();

    if (m_gdbProc.state() == QProcess::NotRunning) {
        debugMessage(_("TRYING TO INTERRUPT INFERIOR WITHOUT RUNNING GDB"));
        qq->notifyInferiorExited();
        return;
    }

    if (q->startMode() == StartRemote) {
        postCommand(_("-exec-interrupt"));
        return;
    }

    const qint64 attachedPID = q->inferiorPid();
    if (attachedPID <= 0) {
        debugMessage(_("TRYING TO INTERRUPT INFERIOR BEFORE PID WAS OBTAINED"));
        return;
    }

    if (!interruptProcess(attachedPID))
        debugMessage(_("CANNOT INTERRUPT %1").arg(attachedPID));
}

void GdbEngine::maybeHandleInferiorPidChanged(const QString &pid0)
{
    const qint64 pid = pid0.toLongLong();
    if (pid == 0) {
        debugMessage(_("Cannot parse PID from %1").arg(pid0));
        return;
    }
    if (pid == q->inferiorPid())
        return;
    debugMessage(_("FOUND PID %1").arg(pid));    

    qq->notifyInferiorPidChanged(pid);
    if (m_dumperInjectionLoad)
        tryLoadDebuggingHelpers();
}

void GdbEngine::postCommand(const QString &command, GdbCommandCallback callback,
                            const char *callbackName, const QVariant &cookie)
{
    postCommand(command, NoFlags, callback, callbackName, cookie);
}

void GdbEngine::postCommand(const QString &command, GdbCommandFlags flags,
                            GdbCommandCallback callback, const char *callbackName,
                            const QVariant &cookie)
{
    if (m_gdbProc.state() == QProcess::NotRunning) {
        debugMessage(_("NO GDB PROCESS RUNNING, CMD IGNORED: ") + command);
        return;
    }

    if (flags & RebuildModel) {
        ++m_pendingRequests;
        PENDING_DEBUG("   CALLBACK" << callbackName << "INCREMENTS PENDING TO:"
            << m_pendingRequests << command);
    } else {
        PENDING_DEBUG("   UNKNOWN CALLBACK" << callbackName << "LEAVES PENDING AT:"
            << m_pendingRequests << command);
    }

    GdbCommand cmd;
    cmd.command = command;
    cmd.flags = flags;
    cmd.callback = callback;
    cmd.callbackName = callbackName;
    cmd.cookie = cookie;

    if ((flags & NeedsStop) && q->status() != DebuggerInferiorStopped
            && q->status() != DebuggerProcessStartingUp) {
        // queue the commands that we cannot send at once
        QTC_ASSERT(q->status() == DebuggerInferiorRunning,
            qDebug() << "STATUS:" << q->status());
        q->showStatusMessage(tr("Stopping temporarily."));
        debugMessage(_("QUEUING COMMAND ") + cmd.command);
        m_commandsToRunOnTemporaryBreak.append(cmd);
        interruptInferior();
    } else if (!command.isEmpty()) {
        flushCommand(cmd);
    }
}

void GdbEngine::flushCommand(GdbCommand &cmd)
{
    ++currentToken();
    m_cookieForToken[currentToken()] = cmd;
    cmd.command = QString::number(currentToken()) + cmd.command;
    if (cmd.flags & EmbedToken)
        cmd.command = cmd.command.arg(currentToken());

    m_gdbProc.write(cmd.command.toLatin1() + "\r\n");
    //emit gdbInputAvailable(QString(), "         " +  currentTime());
    //emit gdbInputAvailable(QString(), "[" + currentTime() + "]    " + cmd.command);
    emit gdbInputAvailable(QString(), cmd.command);
}

void GdbEngine::handleResultRecord(const GdbResultRecord &record)
{
    //qDebug() << "TOKEN:" << record.token
    //    << " ACCEPTABLE:" << m_oldestAcceptableToken;
    //qDebug() << "";
    //qDebug() << "\nRESULT" << record.token << record.toString();

    int token = record.token;
    if (token == -1)
        return;

    GdbCommand cmd = m_cookieForToken.take(token);

    if (record.token < m_oldestAcceptableToken && (cmd.flags & Discardable)) {
        //qDebug() << "### SKIPPING OLD RESULT" << record.toString();
        //QMessageBox::information(m_mainWindow, tr("Skipped"), "xxx");
        return;
    }

#if 0
    qDebug() << "# handleOutput,"
        << "cmd type:" << cmd.type
        << " cmd synchronized:" << cmd.synchronized
        << "\n record: " << record.toString();
#endif

    // << "\n data: " << record.data.toString(true);

    if (cmd.callback)
        (this->*(cmd.callback))(record, cmd.cookie);

    if (cmd.flags & RebuildModel) {
        --m_pendingRequests;
        PENDING_DEBUG("   TYPE " << cmd.type << " DECREMENTS PENDING TO: "
            << m_pendingRequests << cmd.command);
        if (m_pendingRequests <= 0) {
            PENDING_DEBUG(" ....  AND TRIGGERS MODEL UPDATE");
            updateWatchModel2();
        }
    } else {
        PENDING_DEBUG("   UNKNOWN TYPE " << cmd.type << " LEAVES PENDING AT: "
            << m_pendingRequests << cmd.command);
    }

    // This is somewhat inefficient, as it makes the last command synchronous.
    // An optimization would be requesting the continue immediately when the
    // event loop is entered, and let individual commands have a flag to suppress
    // that behavior.
    if (m_cookieForToken.isEmpty() && m_autoContinue) {
        m_autoContinue = false;
        continueInferior();
        q->showStatusMessage(tr("Continuing after temporary stop."));
    }
}

void GdbEngine::executeDebuggerCommand(const QString &command)
{
    if (m_gdbProc.state() == QProcess::NotRunning) {
        debugMessage(_("NO GDB PROCESS RUNNING, PLAIN CMD IGNORED: ") + command);
        return;
    }

    m_gdbProc.write(command.toLocal8Bit() + "\r\n");
}

void GdbEngine::handleTargetCore(const GdbResultRecord &, const QVariant &)
{
    qq->notifyInferiorStopped();
    q->showStatusMessage(tr("Core file loaded."));
    q->resetLocation();
    tryLoadDebuggingHelpers();
    qq->stackHandler()->setCurrentIndex(0);
    updateLocals(); // Quick shot
    reloadStack();
    if (supportsThreads())
        postCommand(_("-thread-list-ids"), WatchUpdate, CB(handleStackListThreads), 0);
    qq->reloadRegisters();
}

void GdbEngine::handleQuerySources(const GdbResultRecord &record, const QVariant &)
{
    if (record.resultClass == GdbResultDone) {
        QMap<QString, QString> oldShortToFull = m_shortToFullName;
        m_shortToFullName.clear();
        m_fullToShortName.clear();
        // "^done,files=[{file="../../../../bin/gdbmacros/gdbmacros.cpp",
        // fullname="/data5/dev/ide/main/bin/gdbmacros/gdbmacros.cpp"},
        GdbMi files = record.data.findChild("files");
        foreach (const GdbMi &item, files.children()) {
            QString fileName = QString::fromLocal8Bit(item.findChild("file").data());
            GdbMi fullName = item.findChild("fullname");
            QString full = QString::fromLocal8Bit(fullName.data());
            #ifdef Q_OS_WIN
            full = QDir::cleanPath(full);
            #endif
            if (fullName.isValid() && QFileInfo(full).isReadable()) {
                //qDebug() << "STORING 2:" << fileName << full;
                m_shortToFullName[fileName] = full;
                m_fullToShortName[full] = fileName;
            }
        }
        if (m_shortToFullName != oldShortToFull)
            qq->sourceFileWindow()->setSourceFiles(m_shortToFullName);
    }
}

void GdbEngine::handleInfoThreads(const GdbResultRecord &record, const QVariant &)
{
    if (record.resultClass != GdbResultDone)
        return;
    // FIXME: use something more robust
    // WIN:     [New thread 3380.0x2bc]
    //          * 3 Thread 2312.0x4d0  0x7c91120f in ?? ()
    // LINUX:   * 1 Thread 0x7f466273c6f0 (LWP 21455)  0x0000000000404542 in ...
    const QString data = _(record.data.findChild("consolestreamoutput").data());
    if (data.isEmpty())
        return;
    // check "[New thread 3380.0x2bc]"
    if (data.startsWith(QLatin1Char('['))) {
        QRegExp ren(_("^\\[New thread (\\d+)\\.0x.*"));
        Q_ASSERT(ren.isValid());
        if (ren.indexIn(data) != -1) {
            maybeHandleInferiorPidChanged(ren.cap(1));
            return;
        }
    }
    // check "* 3 Thread ..."
    QRegExp re(_("^\\*? +\\d+ +[Tt]hread (\\d+)\\.0x.* in"));
    Q_ASSERT(re.isValid());
    if (re.indexIn(data) != -1)
        maybeHandleInferiorPidChanged(re.cap(1));
}

void GdbEngine::handleInfoProc(const GdbResultRecord &record, const QVariant &)
{
    if (record.resultClass == GdbResultDone) {
        #ifdef Q_OS_MAC
        //^done,process-id="85075"
        maybeHandleInferiorPidChanged(_(record.data.findChild("process-id").data()));
        #else
        // FIXME: use something more robust
        QRegExp re(__("process (\\d+)"));
        QString data = __(record.data.findChild("consolestreamoutput").data());
        if (re.indexIn(data) != -1)
            maybeHandleInferiorPidChanged(re.cap(1));
        #endif
    }
}

void GdbEngine::handleInfoShared(const GdbResultRecord &record, const QVariant &cookie)
{
    if (record.resultClass == GdbResultDone) {
        // let the modules handler do the parsing
        handleModulesList(record, cookie);
    }
}

#if 0
void GdbEngine::handleExecJumpToLine(const GdbResultRecord &record)
{
    // FIXME: remove this special case as soon as 'jump'
    // is supported by MI
    // "&"jump /home/apoenitz/dev/work/test1/test1.cpp:242"
    // ~"Continuing at 0x4058f3."
    // ~"run1 (argc=1, argv=0x7fffb213a478) at test1.cpp:242"
    // ~"242\t x *= 2;"
    //109^done"
    qq->notifyInferiorStopped();
    q->showStatusMessage(tr("Jumped. Stopped."));
    QByteArray output = record.data.findChild("logstreamoutput").data();
    if (output.isEmpty())
        return;
    int idx1 = output.indexOf(' ') + 1;
    if (idx1 > 0) {
        int idx2 = output.indexOf(':', idx1);
        if (idx2 > 0) {
            QString file = QString::fromLocal8Bit(output.mid(idx1, idx2 - idx1));
            int line = output.mid(idx2 + 1).toInt();
            q->gotoLocation(file, line, true);
        }
    }
}
#endif

void GdbEngine::handleExecRunToFunction(const GdbResultRecord &record, const QVariant &)
{
    // FIXME: remove this special case as soon as there's a real
    // reason given when the temporary breakpoint is hit.
    // reight now we get:
    // 14*stopped,thread-id="1",frame={addr="0x0000000000403ce4",
    // func="foo",args=[{name="str",value="@0x7fff0f450460"}],
    // file="main.cpp",fullname="/tmp/g/main.cpp",line="37"}
    qq->notifyInferiorStopped();
    q->showStatusMessage(tr("Run to Function finished. Stopped."));
    GdbMi frame = record.data.findChild("frame");
    QString file = QString::fromLocal8Bit(frame.findChild("fullname").data());
    int line = frame.findChild("line").data().toInt();
    qDebug() << "HIT:" << file << line << "IN" << frame.toString()
        << "--" << record.toString();
    q->gotoLocation(file, line, true);
}

static bool isExitedReason(const QByteArray &reason)
{
    return reason == "exited-normally"   // inferior exited normally
        || reason == "exited-signalled"  // inferior exited because of a signal
        //|| reason == "signal-received" // inferior received signal
        || reason == "exited";           // inferior exited
}

static bool isStoppedReason(const QByteArray &reason)
{
    return reason == "function-finished"  // -exec-finish
        || reason == "signal-received"  // handled as "isExitedReason"
        || reason == "breakpoint-hit"     // -exec-continue
        || reason == "end-stepping-range" // -exec-next, -exec-step
        || reason == "location-reached"   // -exec-until
        || reason == "access-watchpoint-trigger"
        || reason == "read-watchpoint-trigger"
        #ifdef Q_OS_MAC
        || reason.isEmpty()
        #endif
    ;
}

void GdbEngine::handleAqcuiredInferior()
{
    #ifdef Q_OS_WIN
    postCommand(_("info thread"), CB(handleInfoThreads));
    #elif defined(Q_OS_MAC)
    postCommand(_("info pid"), NeedsStop, CB(handleInfoProc));
    #else
    postCommand(_("info proc"), CB(handleInfoProc));
    #endif
    if (theDebuggerBoolSetting(ListSourceFiles))
        reloadSourceFiles();

    // Reverse debugging. FIXME: Should only be used when available.
    //if (theDebuggerBoolSetting(EnableReverseDebugging))
    //    postCommand(_("target record"));

    tryLoadDebuggingHelpers();

    #ifndef Q_OS_MAC
    // intentionally after tryLoadDebuggingHelpers(),
    // otherwise we'd interupt solib loading.
    if (theDebuggerBoolSetting(AllPluginBreakpoints)) {
        postCommand(_("set auto-solib-add on"));
        postCommand(_("set stop-on-solib-events 0"));
        postCommand(_("sharedlibrary .*"));
    } else if (theDebuggerBoolSetting(SelectedPluginBreakpoints)) {
        postCommand(_("set auto-solib-add on"));
        postCommand(_("set stop-on-solib-events 1"));
        postCommand(_("sharedlibrary ")
          + theDebuggerStringSetting(SelectedPluginBreakpointsPattern));
    } else if (theDebuggerBoolSetting(NoPluginBreakpoints)) {
        // should be like that already
        if (!m_dumperInjectionLoad)
            postCommand(_("set auto-solib-add off"));
        postCommand(_("set stop-on-solib-events 0"));
    }
    #endif

    // nicer to see a bit of the world we live in
    reloadModules();
    attemptBreakpointSynchronization();
}

void GdbEngine::handleAsyncOutput(const GdbMi &data)
{
    const QByteArray &reason = data.findChild("reason").data();

    if (isExitedReason(reason)) {
        qq->notifyInferiorExited();
        QString msg;
        if (reason == "exited") {
            msg = tr("Program exited with exit code %1")
                .arg(_(data.findChild("exit-code").toString()));
        } else if (reason == "exited-signalled" || reason == "signal-received") {
            msg = tr("Program exited after receiving signal %1")
                .arg(_(data.findChild("signal-name").toString()));
        } else {
            msg = tr("Program exited normally");
        }
        q->showStatusMessage(msg);
        postCommand(_("-gdb-exit"), CB(handleExit));
        return;
    }


    //MAC: bool isFirstStop = data.findChild("bkptno").data() == "1";
    //!MAC: startSymbolName == data.findChild("frame").findChild("func")
    if (m_waitingForFirstBreakpointToBeHit) {
        m_waitingForFirstBreakpointToBeHit = false;

        // If the executable dies already that early we might get something
        // like stdout:49*stopped,reason="exited",exit-code="0177"
        // This is handled now above.

        qq->notifyInferiorStopped();
        handleAqcuiredInferior();
        m_autoContinue = true;
        return;
    }

    if (!m_commandsToRunOnTemporaryBreak.isEmpty()) {
        QTC_ASSERT(q->status() == DebuggerInferiorStopRequested,
            qDebug() << "STATUS:" << q->status())
        qq->notifyInferiorStopped();
        // FIXME: racy
        while (!m_commandsToRunOnTemporaryBreak.isEmpty()) {
            GdbCommand cmd = m_commandsToRunOnTemporaryBreak.takeFirst();
            debugMessage(_("RUNNING QUEUED COMMAND %1 %2")
                .arg(cmd.command).arg(_(cmd.callbackName)));
            flushCommand(cmd);
        }
        q->showStatusMessage(tr("Processing queued commands."));
        m_autoContinue = true;
        return;
    }

    const QByteArray &msg = data.findChild("consolestreamoutput").data();
    if (msg.contains("Stopped due to shared library event") || reason.isEmpty()) {
        if (theDebuggerBoolSetting(SelectedPluginBreakpoints)) {
            QString dataStr = _(data.toString());
            debugMessage(_("SHARED LIBRARY EVENT: ") + dataStr);
            QString pat = theDebuggerStringSetting(SelectedPluginBreakpointsPattern);
            debugMessage(_("PATTERN: ") + pat);
            postCommand(_("sharedlibrary ") + pat);
            continueInferior();
            q->showStatusMessage(tr("Loading %1...").arg(dataStr));
            return;
        }
        m_modulesListOutdated = true;
        // fall through
    }

    // seen on XP after removing a breakpoint while running
    //  stdout:945*stopped,reason="signal-received",signal-name="SIGTRAP",
    //  signal-meaning="Trace/breakpoint trap",thread-id="2",
    //  frame={addr="0x7c91120f",func="ntdll!DbgUiConnectToDbg",
    //  args=[],from="C:\\WINDOWS\\system32\\ntdll.dll"}
    if (reason == "signal-received"
          && data.findChild("signal-name").toString() == "SIGTRAP") {
        continueInferior();
        return;
    }

    // jump over well-known frames
    static int stepCounter = 0;
    if (theDebuggerBoolSetting(SkipKnownFrames)) {
        if (reason == "end-stepping-range" || reason == "function-finished") {
            GdbMi frame = data.findChild("frame");
            //debugMessage(frame.toString());
            m_currentFrame = _(frame.findChild("addr").data() + '%' +
                 frame.findChild("func").data() + '%');

            QString funcName = _(frame.findChild("func").data());
            QString fileName = QString::fromLocal8Bit(frame.findChild("file").data());
            if (isLeavableFunction(funcName, fileName)) {
                //debugMessage(_("LEAVING ") + funcName);
                ++stepCounter;
                q->stepOutExec();
                //stepExec();
                return;
            }
            if (isSkippableFunction(funcName, fileName)) {
                //debugMessage(_("SKIPPING ") + funcName);
                ++stepCounter;
                q->stepExec();
                return;
            }
            //if (stepCounter)
            //    qDebug() << "STEPCOUNTER:" << stepCounter;
            stepCounter = 0;
        }
    }

    if (isStoppedReason(reason) || reason.isEmpty()) {
        if (m_modulesListOutdated) {
            reloadModules();
            m_modulesListOutdated = false;
        }
        // Need another round trip
        if (reason == "breakpoint-hit") {
            q->showStatusMessage(tr("Stopped at breakpoint."));
            GdbMi frame = data.findChild("frame");
            //debugMessage(_("HIT BREAKPOINT: " + frame.toString()));
            m_currentFrame = _(frame.findChild("addr").data() + '%' +
                 frame.findChild("func").data() + '%');

            QApplication::alert(q->mainWindow(), 3000);
            if (theDebuggerAction(ListSourceFiles)->value().toBool())
                reloadSourceFiles();
            postCommand(_("-break-list"), CB(handleBreakList));
            QVariant var = QVariant::fromValue<GdbMi>(data);
            postCommand(_("p 0"), CB(handleAsyncOutput2), var);  // dummy
        } else {
#ifdef Q_OS_LINUX
            // For some reason, attaching to a stopped process causes *two* stops
            // when trying to continue (kernel 2.6.24-23-ubuntu).
            // Interestingly enough, on MacOSX no signal is delivered at all.
            if (reason == "signal-received"
                && data.findChild("signal-name").data() == "SIGSTOP") {
                GdbMi frameData = data.findChild("frame");
                if (frameData.findChild("func").data() == "_start"
                    && frameData.findChild("from").data() == "/lib/ld-linux.so.2") {
                    postCommand(_("-exec-continue"));
                    return;
                }
            }
#endif
            if (reason.isEmpty())
                q->showStatusMessage(tr("Stopped."));
            else
                q->showStatusMessage(tr("Stopped: \"%1\"").arg(_(reason)));
            handleAsyncOutput2(data);
        }
        return;
    }

    debugMessage(_("STOPPED FOR UNKNOWN REASON: " + data.toString()));
    // Ignore it. Will be handled with full response later in the
    // JumpToLine or RunToFunction handlers
#if 1
    // FIXME: remove this special case as soon as there's a real
    // reason given when the temporary breakpoint is hit.
    // reight now we get:
    // 14*stopped,thread-id="1",frame={addr="0x0000000000403ce4",
    // func="foo",args=[{name="str",value="@0x7fff0f450460"}],
    // file="main.cpp",fullname="/tmp/g/main.cpp",line="37"}
    //
    // MAC yields sometimes:
    // stdout:3661*stopped,time={wallclock="0.00658",user="0.00142",
    // system="0.00136",start="1218810678.805432",end="1218810678.812011"}
    q->resetLocation();
    qq->notifyInferiorStopped();
    q->showStatusMessage(tr("Run to Function finished. Stopped."));
    GdbMi frame = data.findChild("frame");
    QString file = QString::fromLocal8Bit(frame.findChild("fullname").data());
    int line = frame.findChild("line").data().toInt();
    qDebug() << "HIT:" << file << line << "IN" << frame.toString()
        << "--" << data.toString();
    q->gotoLocation(file, line, true);
#endif
}

void GdbEngine::reloadFullStack()
{
    QString cmd = _("-stack-list-frames");
    postCommand(cmd, WatchUpdate, CB(handleStackListFrames), true);
}

void GdbEngine::reloadStack()
{
    QString cmd = _("-stack-list-frames");
    if (int stackDepth = theDebuggerAction(MaximalStackDepth)->value().toInt())
        cmd += _(" 0 ") + QString::number(stackDepth);
    postCommand(cmd, WatchUpdate, CB(handleStackListFrames), false);
}

void GdbEngine::handleAsyncOutput2(const GdbResultRecord &, const QVariant &cookie)
{
    handleAsyncOutput2(cookie.value<GdbMi>());
}

void GdbEngine::handleAsyncOutput2(const GdbMi &data)
{
    qq->notifyInferiorStopped();

    //
    // Stack
    //
    qq->stackHandler()->setCurrentIndex(0);
    updateLocals(); // Quick shot

    int currentId = data.findChild("thread-id").data().toInt();
    reloadStack();
    if (supportsThreads())
        postCommand(_("-thread-list-ids"), WatchUpdate, CB(handleStackListThreads), currentId);

    //
    // Disassembler
    //
    // Linux:
    //"79*stopped,reason="end-stepping-range",reason="breakpoint-hit",bkptno="1",
    //thread-id="1",frame={addr="0x0000000000405d8f",func="run1",
    //args=[{name="argc",value="1"},{name="argv",value="0x7fffb7c23058"}],
    //file="test1.cpp",fullname="/home/apoenitz/dev/work/test1/test1.cpp",line="261"}"
    // Mac: (but only sometimes)
    m_address = _(data.findChild("frame").findChild("addr").data());
    qq->reloadDisassembler();

    //
    // Registers
    //
    qq->reloadRegisters();
}

void GdbEngine::handleShowVersion(const GdbResultRecord &response, const QVariant &)
{
    //qDebug () << "VERSION 2:" << response.data.findChild("consolestreamoutput").data();
    //qDebug () << "VERSION:" << response.toString();
    debugMessage(_("VERSION: " + response.toString()));
    if (response.resultClass == GdbResultDone) {
        m_gdbVersion = 100;
        m_gdbBuildVersion = -1;
        QString msg = QString::fromLocal8Bit(response.data.findChild("consolestreamoutput").data());
        QRegExp supported(_("GNU gdb(.*) (\\d+)\\.(\\d+)(\\.(\\d+))?(-(\\d+))?"));
        if (supported.indexIn(msg) == -1) {
            debugMessage(_("UNSUPPORTED GDB VERSION ") + msg);
            QStringList list = msg.split(_c('\n'));
            while (list.size() > 2)
                list.removeLast();
            msg = tr("The debugger you are using identifies itself as:")
                + _("<p><p>") + list.join(_("<br>")) + _("<p><p>")
                + tr("This version is not officially supported by Qt Creator.\n"
                     "Debugging will most likely not work well.\n"
                     "Using gdb 6.7 or later is strongly recommended.");
#if 0
            // ugly, but 'Show again' check box...
            static QErrorMessage *err = new QErrorMessage(m_mainWindow);
            err->setMinimumSize(400, 300);
            err->showMessage(msg);
#else
            //QMessageBox::information(m_mainWindow, tr("Warning"), msg);
#endif
        } else {
            m_gdbVersion = 10000 * supported.cap(2).toInt()
                         +   100 * supported.cap(3).toInt()
                         +     1 * supported.cap(5).toInt();
            m_gdbBuildVersion = supported.cap(7).toInt();
            debugMessage(_("GDB VERSION: %1").arg(m_gdbVersion));
        }
        //qDebug () << "VERSION 3:" << m_gdbVersion << m_gdbBuildVersion;
    }
}

void GdbEngine::handleFileExecAndSymbols(const GdbResultRecord &response, const QVariant &)
{
    if (response.resultClass == GdbResultDone) {
        //m_breakHandler->clearBreakMarkers();
    } else if (response.resultClass == GdbResultError) {
        QString msg = __(response.data.findChild("msg").data());
        QMessageBox::critical(q->mainWindow(), tr("Error"),
            tr("Starting executable failed:\n") + msg);
        QTC_ASSERT(q->status() == DebuggerInferiorRunning, /**/);
        interruptInferior();
    }
}

void GdbEngine::handleExecRun(const GdbResultRecord &response, const QVariant &)
{
    if (response.resultClass == GdbResultRunning) {
        qq->notifyInferiorRunning();
    } else if (response.resultClass == GdbResultError) {
        const QByteArray &msg = response.data.findChild("msg").data();
        if (msg == "Cannot find bounds of current function") {
            qq->notifyInferiorStopped();
            //q->showStatusMessage(tr("No debug information available. "
            //  "Leaving function..."));
            //stepOutExec();
        } else {
            QMessageBox::critical(q->mainWindow(), tr("Error"),
                tr("Starting executable failed:\n") + QString::fromLocal8Bit(msg));
            QTC_ASSERT(q->status() == DebuggerInferiorRunning, /**/);
            interruptInferior();
        }
    }
}

QString GdbEngine::fullName(const QString &fileName)
{
    //QString absName = m_manager->currentWorkingDirectory() + "/" + file; ??
    if (fileName.isEmpty())
        return QString();
    QString full = m_shortToFullName.value(fileName, QString());
    //debugMessage(_("RESOLVING: ") + fileName + " " +  full);
    if (!full.isEmpty())
        return full;
    QFileInfo fi(fileName);
    if (!fi.isReadable())
        return QString();
    full = fi.absoluteFilePath();
    #ifdef Q_OS_WIN
    full = QDir::cleanPath(full);
    #endif
    //debugMessage(_("STORING: ") + fileName + " " + full);
    m_shortToFullName[fileName] = full;
    m_fullToShortName[full] = fileName;
    return full;
}

QString GdbEngine::fullName(const QStringList &candidates)
{
    QString full;
    foreach (const QString &fileName, candidates) {
        full = fullName(fileName);
        if (!full.isEmpty())
            return full;
    }
    foreach (const QString &fileName, candidates) {
        if (!fileName.isEmpty())
            return fileName;
    }
    return full;
}

void GdbEngine::shutdown()
{
    exitDebugger();
}

void GdbEngine::detachDebugger()
{
    postCommand(_("detach"));
    postCommand(_("-gdb-exit"), CB(handleExit));
}

void GdbEngine::exitDebugger()
{
    debugMessage(_("GDBENGINE EXITDEBUGGER: %1").arg(m_gdbProc.state()));
    if (m_gdbProc.state() == QProcess::Starting) {
        debugMessage(_("WAITING FOR GDB STARTUP TO SHUTDOWN: %1")
            .arg(m_gdbProc.state()));
        m_gdbProc.waitForStarted();
    }
    if (m_gdbProc.state() == QProcess::Running) {
        debugMessage(_("WAITING FOR RUNNING GDB TO SHUTDOWN: %1")
            .arg(m_gdbProc.state()));
        if (q->status() != DebuggerInferiorStopped
            && q->status() != DebuggerProcessStartingUp) {
            QTC_ASSERT(q->status() == DebuggerInferiorRunning,
                qDebug() << "STATUS ON EXITDEBUGGER:" << q->status());
            interruptInferior();
        }
        if (q->startMode() == AttachExternal || q->startMode() == AttachCrashedExternal)
            postCommand(_("detach"));
        else
            postCommand(_("kill"));
        postCommand(_("-gdb-exit"), CB(handleExit));
        // 20s can easily happen when loading webkit debug information
        if (!m_gdbProc.waitForFinished(20000)) {
            debugMessage(_("FORCING TERMINATION: %1")
                .arg(m_gdbProc.state()));
            m_gdbProc.terminate();
            m_gdbProc.waitForFinished(20000);
        }
    }
    if (m_gdbProc.state() != QProcess::NotRunning) {
        debugMessage(_("PROBLEM STOPPING DEBUGGER: STATE %1")
            .arg(m_gdbProc.state()));
        m_gdbProc.kill();
    }

    m_outputCollector.shutdown();
    initializeVariables();
    //q->settings()->m_debugDebuggingHelpers = false;
}


int GdbEngine::currentFrame() const
{
    return qq->stackHandler()->currentIndex();
}

bool GdbEngine::startDebugger(const QSharedPointer<DebuggerStartParameters> &sp)
{
    debugMessage(DebuggerSettings::instance()->dump());
    QStringList gdbArgs;

    if (m_gdbProc.state() != QProcess::NotRunning) {
        debugMessage(_("GDB IS ALREADY RUNNING, STATE: %1").arg(m_gdbProc.state()));
        m_gdbProc.kill();
        return false;
    }

    //gdbArgs.prepend(_("--quiet"));
    gdbArgs.prepend(_("mi"));
    gdbArgs.prepend(_("-i"));

    if (q->startMode() == AttachCore || q->startMode() == AttachExternal || q->startMode() == AttachCrashedExternal) {
        // nothing to do
    } else if (q->startMode() == StartRemote) {
        // Start the remote server
        if (sp->serverStartScript.isEmpty()) {
            q->showStatusMessage(_("No server start script given. "
                "Assuming server runs already."));
        } else {
            if (!sp->workingDir.isEmpty())
                m_uploadProc.setWorkingDirectory(sp->workingDir);
            if (!sp->environment.isEmpty())
                m_uploadProc.setEnvironment(sp->environment);
            m_uploadProc.start(_("/bin/sh ") + sp->serverStartScript);
            m_uploadProc.waitForStarted();
        }
    } else if (sp->useTerminal) {
        m_stubProc.stop(); // We leave the console open, so recycle it now.

        m_stubProc.setWorkingDirectory(sp->workingDir);
        m_stubProc.setEnvironment(sp->environment);
        if (!m_stubProc.start(sp->executable, sp->processArgs))
            return false; // Error message for user is delivered via a signal.
    } else {
        if (!m_outputCollector.listen()) {
            QMessageBox::critical(q->mainWindow(), tr("Debugger Startup Failure"),
                tr("Cannot set up communication with child process: %1")
                    .arg(m_outputCollector.errorString()));
            return false;
        }
        gdbArgs.prepend(_("--tty=") + m_outputCollector.serverName());

        if (!sp->workingDir.isEmpty())
            m_gdbProc.setWorkingDirectory(sp->workingDir);
        if (!sp->environment.isEmpty())
            m_gdbProc.setEnvironment(sp->environment);
    }

    #if 0
    qDebug() << "Command:" << q->settings()->m_gdbCmd;
    qDebug() << "WorkingDirectory:" << m_gdbProc.workingDirectory();
    qDebug() << "ScriptFile:" << q->settings()->m_scriptFile;
    qDebug() << "Environment:" << m_gdbProc.environment();
    qDebug() << "Arguments:" << gdbArgs;
    qDebug() << "BuildDir:" << sp->buildDir;
    qDebug() << "ExeFile:" << sp->executable;
    #endif

    QString loc = theDebuggerStringSetting(GdbLocation);
    q->showStatusMessage(tr("Starting Debugger: ") + loc + _c(' ') + gdbArgs.join(_(" ")));
    m_gdbProc.start(loc, gdbArgs);
    if (!m_gdbProc.waitForStarted()) {
        QMessageBox::critical(q->mainWindow(), tr("Debugger Startup Failure"),
                              tr("Cannot start debugger: %1").arg(m_gdbProc.errorString()));
        m_outputCollector.shutdown();
        m_stubProc.blockSignals(true);
        m_stubProc.stop();
        m_stubProc.blockSignals(false);
        return false;
    }

    q->showStatusMessage(tr("Gdb Running..."));

    postCommand(_("show version"), CB(handleShowVersion));
    //postCommand(_("-enable-timings");
    postCommand(_("set print static-members off")); // Seemingly doesn't work.
    //postCommand(_("set debug infrun 1"));
    //postCommand(_("define hook-stop\n-thread-list-ids\n-stack-list-frames\nend"));
    //postCommand(_("define hook-stop\nprint 4\nend"));
    //postCommand(_("define hookpost-stop\nprint 5\nend"));
    //postCommand(_("define hook-call\nprint 6\nend"));
    //postCommand(_("define hookpost-call\nprint 7\nend"));
    //postCommand(_("set print object on")); // works with CLI, but not MI
    //postCommand(_("set step-mode on"));  // we can't work with that yes
    //postCommand(_("set exec-done-display on"));
    //postCommand(_("set print pretty on"));
    //postCommand(_("set confirm off"));
    //postCommand(_("set pagination off"));
    postCommand(_("set print inferior-events 1"));
    postCommand(_("set breakpoint pending on"));
    postCommand(_("set print elements 10000"));
    postCommand(_("-data-list-register-names"), CB(handleRegisterListNames));

    //postCommand(_("set substitute-path /var/tmp/qt-x11-src-4.5.0 "
    //    "/home/sandbox/qtsdk-2009.01/qt"));

    // one of the following is needed to prevent crashes in gdb on code like:
    //  template <class T> T foo() { return T(0); }
    //  int main() { return foo<int>(); }
    //  (gdb) call 'int foo<int>'()
    //  /build/buildd/gdb-6.8/gdb/valops.c:2069: internal-error:
    postCommand(_("set overload-resolution off"));
    //postCommand(_("set demangle-style none"));

    // From the docs:
    //  Stop means reenter debugger if this signal happens (implies print).
    //  Print means print a message if this signal happens.
    //  Pass means let program see this signal;
    //  otherwise program doesn't know.
    //  Pass and Stop may be combined.
    // We need "print" as otherwise we would get no feedback whatsoever
    // Custom DebuggingHelper crashs which happen regularily for when accessing
    // uninitialized variables.
    postCommand(_("handle SIGSEGV nopass stop print"));

    // This is useful to kill the inferior whenever gdb dies.
    //postCommand(_("handle SIGTERM pass nostop print"));

    postCommand(_("set unwindonsignal on"));
    //postCommand(_("pwd"));
    postCommand(_("set width 0"));
    postCommand(_("set height 0"));

    #ifdef Q_OS_MAC
    postCommand(_("-gdb-set inferior-auto-start-cfm off"));
    postCommand(_("-gdb-set sharedLibrary load-rules "
            "dyld \".*libSystem.*\" all "
            "dyld \".*libauto.*\" all "
            "dyld \".*AppKit.*\" all "
            "dyld \".*PBGDBIntrospectionSupport.*\" all "
            "dyld \".*Foundation.*\" all "
            "dyld \".*CFDataFormatters.*\" all "
            "dyld \".*libobjc.*\" all "
            "dyld \".*CarbonDataFormatters.*\" all"));
    #endif

    QString scriptFileName = theDebuggerStringSetting(GdbScriptFile);
    if (!scriptFileName.isEmpty()) {
        if (QFileInfo(scriptFileName).isReadable()) {
            postCommand(_("source ") + scriptFileName);
        } else {
            QMessageBox::warning(q->mainWindow(),
            tr("Cannot find debugger initialization script"),
            tr("The debugger settings point to a script file at '%1' "
               "which is not accessible. If a script file is not needed, "
               "consider clearing that entry to avoid this warning. "
              ).arg(scriptFileName));
        }
    }

    if (q->startMode() == AttachExternal || q->startMode() == AttachCrashedExternal) {
        postCommand(_("attach %1").arg(sp->attachPID), CB(handleAttach));
        qq->breakHandler()->removeAllBreakpoints();
    } else if (q->startMode() == AttachCore) {
        QFileInfo fi(sp->executable);
        QString fileName = _c('"') + fi.absoluteFilePath() + _c('"');
        QFileInfo fi2(sp->coreFile);
        // quoting core name below fails in gdb 6.8-debian
        QString coreName = fi2.absoluteFilePath();
        postCommand(_("-file-exec-and-symbols ") + fileName);
        postCommand(_("target core ") + coreName, CB(handleTargetCore));
        qq->breakHandler()->removeAllBreakpoints();
    } else if (q->startMode() == StartRemote) {
        postCommand(_("set architecture %1").arg(sp->remoteArchitecture));
        qq->breakHandler()->setAllPending();
        //QFileInfo fi(sp->executable);
        //QString fileName = fi.absoluteFileName();
        QString fileName = sp->executable;
        postCommand(_("-file-exec-and-symbols \"%1\"").arg(fileName));
        // works only for > 6.8
        postCommand(_("set target-async on"), CB(handleSetTargetAsync));
    } else if (sp->useTerminal) {
        qq->breakHandler()->setAllPending();
    } else if (q->startMode() == StartInternal || q->startMode() == StartExternal) {
        QFileInfo fi(sp->executable);
        QString fileName = _c('"') + fi.absoluteFilePath() + _c('"');
        postCommand(_("-file-exec-and-symbols ") + fileName, CB(handleFileExecAndSymbols));
        //postCommand(_("file ") + fileName, handleFileExecAndSymbols);
        #ifdef Q_OS_MAC
        postCommand(_("sharedlibrary apply-load-rules all"));
        #endif
        if (!sp->processArgs.isEmpty())
            postCommand(_("-exec-arguments ") + sp->processArgs.join(_(" ")));
        #ifndef Q_OS_MAC        
        if (!m_dumperInjectionLoad)
            postCommand(_("set auto-solib-add off"));
        postCommand(_("info target"), CB(handleStart));
        #else
        // On MacOS, breaking in at the entry point wreaks havoc.
        postCommand(_("tbreak main"));
        m_waitingForFirstBreakpointToBeHit = true;
        qq->notifyInferiorRunningRequested();
        postCommand(_("-exec-run"));
        #endif
        qq->breakHandler()->setAllPending();
    }

    return true;
}

void GdbEngine::continueInferior()
{
    q->resetLocation();
    setTokenBarrier();
    qq->notifyInferiorRunningRequested();
    postCommand(_("-exec-continue"), CB(handleExecRun));
}

void GdbEngine::handleStart(const GdbResultRecord &response, const QVariant &)
{
#if defined(Q_OS_MAC)
    Q_UNUSED(response);
#else
    if (response.resultClass == GdbResultDone) {
        // [some leading stdout here]
        // stdout:&"        Entry point: 0x80831f0  0x08048134 - 0x08048147 is .interp\n"
        // [some trailing stdout here]
        QString msg = _(response.data.findChild("consolestreamoutput").data());
        QRegExp needle(_("\\bEntry point: (0x[0-9a-f]+)\\b"));
        if (needle.indexIn(msg) != -1) {
            //debugMessage(_("STREAM: ") + msg + " " + needle.cap(1));
            postCommand(_("tbreak *") + needle.cap(1));
            m_waitingForFirstBreakpointToBeHit = true;
            qq->notifyInferiorRunningRequested();
            postCommand(_("-exec-run"));
        } else {
            debugMessage(_("PARSING START ADDRESS FAILED: ") + msg);
        }
    } else if (response.resultClass == GdbResultError) {
        debugMessage(_("FETCHING START ADDRESS FAILED: " + response.toString()));
    }
#endif
}

void GdbEngine::handleAttach(const GdbResultRecord &, const QVariant &)
{
    qq->notifyInferiorStopped();
    q->showStatusMessage(tr("Attached to running process. Stopped."));
    handleAqcuiredInferior();

    q->resetLocation();
    recheckDebuggingHelperAvailability();

    //
    // Stack
    //
    qq->stackHandler()->setCurrentIndex(0);
    updateLocals(); // Quick shot

    reloadStack();
    if (supportsThreads())
        postCommand(_("-thread-list-ids"), WatchUpdate, CB(handleStackListThreads), 0);

    //
    // Disassembler
    //
    // XXX we have no data here ...
    //m_address = data.findChild("frame").findChild("addr").data();
    //qq->reloadDisassembler();

    //
    // Registers
    //
    qq->reloadRegisters();
}

void GdbEngine::handleSetTargetAsync(const GdbResultRecord &record, const QVariant &)
{
    if (record.resultClass == GdbResultDone) {
        //postCommand(_("info target"), handleStart);
        qq->notifyInferiorRunningRequested();
        postCommand(_("target remote %1").arg(q->startParameters()->remoteChannel),
            CB(handleTargetRemote));
    } else if (record.resultClass == GdbResultError) {
        // a typical response on "old" gdb is:
        // &"set target-async on\n"
        //&"No symbol table is loaded.  Use the \"file\" command.\n"
        //^error,msg="No symbol table is loaded.  Use the \"file\" command."
        postCommand(_("detach"));
        postCommand(_("-gdb-exit"), CB(handleExit));
    }
}

void GdbEngine::handleTargetRemote(const GdbResultRecord &record, const QVariant &)
{
    if (record.resultClass == GdbResultDone) {
        //postCommand(_("-exec-continue"), CB(handleExecRun));
        handleAqcuiredInferior();
        m_autoContinue = true;
    } else if (record.resultClass == GdbResultError) {
        // 16^error,msg="hd:5555: Connection timed out."
        QString msg = __(record.data.findChild("msg").data());
        QString msg1 = tr("Connecting to remote server failed:");
        q->showStatusMessage(msg1 + _c(' ') + msg);
        QMessageBox::critical(q->mainWindow(), tr("Error"), msg1 + _c('\n') + msg);
        postCommand(_("-gdb-exit"), CB(handleExit));
    }
}

void GdbEngine::handleExit(const GdbResultRecord &, const QVariant &)
{
    q->showStatusMessage(tr("Debugger exited."));
}

void GdbEngine::stepExec()
{
    setTokenBarrier();
    qq->notifyInferiorRunningRequested();
    if (qq->isReverseDebugging())
        postCommand(_("reverse-step"), CB(handleExecRun));
    else
        postCommand(_("-exec-step"), CB(handleExecRun));
}

void GdbEngine::stepIExec()
{
    setTokenBarrier();
    qq->notifyInferiorRunningRequested();
    if (qq->isReverseDebugging())
        postCommand(_("reverse-stepi"), CB(handleExecRun));
    else
        postCommand(_("-exec-step-instruction"), CB(handleExecRun));
}

void GdbEngine::stepOutExec()
{
    setTokenBarrier();
    qq->notifyInferiorRunningRequested();
    postCommand(_("-exec-finish"), CB(handleExecRun));
}

void GdbEngine::nextExec()
{
    setTokenBarrier();
    qq->notifyInferiorRunningRequested();
    if (qq->isReverseDebugging())
        postCommand(_("reverse-next"), CB(handleExecRun));
    else
        postCommand(_("-exec-next"), CB(handleExecRun));
}

void GdbEngine::nextIExec()
{
    setTokenBarrier();
    qq->notifyInferiorRunningRequested();
    if (qq->isReverseDebugging())
        postCommand(_("reverse-nexti"), CB(handleExecRun));
    else
        postCommand(_("exec-next-instruction"), CB(handleExecRun));
}

void GdbEngine::runToLineExec(const QString &fileName, int lineNumber)
{
    setTokenBarrier();
    qq->notifyInferiorRunningRequested();
    postCommand(_("-exec-until %1:%2").arg(fileName).arg(lineNumber));
}

void GdbEngine::runToFunctionExec(const QString &functionName)
{
    setTokenBarrier();
    postCommand(_("-break-insert -t ") + functionName);
    qq->notifyInferiorRunningRequested();
    // that should be "^running". We need to handle the resulting
    // "Stopped"
    postCommand(_("-exec-continue"));
    //postCommand(_("-exec-continue"), handleExecRunToFunction);
}

void GdbEngine::jumpToLineExec(const QString &fileName, int lineNumber)
{
#if 1
    // not available everywhere?
    //sendCliCommand(_("tbreak ") + fileName + ':' + QString::number(lineNumber));
    postCommand(_("-break-insert -t ") + fileName + _c(':') + QString::number(lineNumber));
    postCommand(_("jump ") + fileName + _c(':') + QString::number(lineNumber));
    // will produce something like
    //  &"jump /home/apoenitz/dev/work/test1/test1.cpp:242"
    //  ~"Continuing at 0x4058f3."
    //  ~"run1 (argc=1, argv=0x7fffbf1f5538) at test1.cpp:242"
    //  ~"242\t x *= 2;"
    //  23^done"
    q->gotoLocation(fileName, lineNumber, true);
    //setBreakpoint();
    //postCommand(_("jump ") + fileName + ':' + QString::number(lineNumber));
#else
    q->gotoLocation(fileName, lineNumber, true);
    setBreakpoint(fileName, lineNumber);
    postCommand(_("jump ") + fileName + ':' + QString::number(lineNumber));
#endif
}

/*!
    \fn void GdbEngine::setTokenBarrier()
    \brief Discard the results of all pending watch-updating commands.

    This method is called at the beginning of all step/next/finish etc.
    debugger functions.
    If non-watch-updating commands with call-backs are still in the pipe,
    it will complain.
*/

void GdbEngine::setTokenBarrier()
{
    foreach (const GdbCommand &cookie, m_cookieForToken) {
        QTC_ASSERT(!cookie.callback || (cookie.flags & Discardable),
            qDebug() << "CMD:" << cookie.command << " CALLBACK:" << cookie.callbackName;
            return
        );
    }
    PENDING_DEBUG("\n--- token barrier ---\n");
    emit gdbInputAvailable(QString(), _("--- token barrier ---"));
    m_oldestAcceptableToken = currentToken();
}

void GdbEngine::setDebugDebuggingHelpers(const QVariant &on)
{
    if (on.toBool()) {
        debugMessage(_("SWITCHING ON DUMPER DEBUGGING"));
        postCommand(_("set unwindonsignal off"));
        q->breakByFunction(_("qDumpObjectData440"));
        //updateLocals();
    } else {
        debugMessage(_("SWITCHING OFF DUMPER DEBUGGING"));
        postCommand(_("set unwindonsignal on"));
    }
}


//////////////////////////////////////////////////////////////////////
//
// Breakpoint specific stuff
//
//////////////////////////////////////////////////////////////////////

void GdbEngine::breakpointDataFromOutput(BreakpointData *data, const GdbMi &bkpt)
{
    if (!bkpt.isValid())
        return;
    if (!data)
        return;
    data->pending = false;
    data->bpMultiple = false;
    data->bpEnabled = true;
    data->bpCondition.clear();
    QStringList files;
    foreach (const GdbMi &child, bkpt.children()) {
        if (child.hasName("number")) {
            data->bpNumber = _(child.data());
        } else if (child.hasName("func")) {
            data->bpFuncName = _(child.data());
        } else if (child.hasName("addr")) {
            // <MULTIPLE> happens in constructors. In this case there are
            // _two_ fields named "addr" in the response. On Linux that is...
            if (child.data() == "<MULTIPLE>")
                data->bpMultiple = true;
            else
                data->bpAddress = _(child.data());
        } else if (child.hasName("file")) {
            files.append(QString::fromLocal8Bit(child.data()));
        } else if (child.hasName("fullname")) {
            QString fullName = QString::fromLocal8Bit(child.data());
            #ifdef Q_OS_WIN
            fullName = QDir::cleanPath(fullName);
            #endif
            files.prepend(fullName);
        } else if (child.hasName("line")) {
            data->bpLineNumber = _(child.data());
            if (child.data().toInt())
                data->markerLineNumber = child.data().toInt();
        } else if (child.hasName("cond")) {
            data->bpCondition = _(child.data());
            // gdb 6.3 likes to "rewrite" conditions. Just accept that fact.
            if (data->bpCondition != data->condition && data->conditionsMatch())
                data->condition = data->bpCondition;
        } else if (child.hasName("enabled")) {
            data->bpEnabled = (child.data() == "y");
        }
        else if (child.hasName("pending")) {
            data->pending = true;
            int pos = child.data().lastIndexOf(':');
            if (pos > 0) {
                data->bpLineNumber = _(child.data().mid(pos + 1));
                data->markerLineNumber = child.data().mid(pos + 1).toInt();
                QString file = QString::fromLocal8Bit(child.data().left(pos));
                if (file.startsWith(_c('"')) && file.endsWith(_c('"')))
                    file = file.mid(1, file.size() - 2);
                files.prepend(file);
            } else {
                files.prepend(QString::fromLocal8Bit(child.data()));
            }
        }
    }
    // This field is not present.  Contents needs to be parsed from
    // the plain "ignore" response.
    //else if (child.hasName("ignore"))
    //    data->bpIgnoreCount = child.data();

    QString name = fullName(files);
    if (data->bpFileName.isEmpty())
        data->bpFileName = name;
    if (data->markerFileName.isEmpty())
        data->markerFileName = name;
}

void GdbEngine::sendInsertBreakpoint(int index)
{
    const BreakpointData *data = qq->breakHandler()->at(index);
    QString where;
    if (data->funcName.isEmpty()) {
        if (data->useFullPath) {
            where = data->fileName;
        } else {
            QFileInfo fi(data->fileName);
            where = fi.fileName();
        }
        // The argument is simply a C-quoted version of the argument to the
        // non-MI "break" command, including the "original" quoting it wants.
        where = _("\"\\\"") + GdbMi::escapeCString(where) + _("\\\":") + data->lineNumber + _c('"');
    } else {
        where = data->funcName;
    }

    // set up fallback in case of pending breakpoints which aren't handled
    // by the MI interface
#if defined(Q_OS_WIN)
    QString cmd = _("-break-insert ");
    //if (!data->condition.isEmpty())
    //    cmd += "-c " + data->condition + " ";
    cmd += where;
#elif defined(Q_OS_MAC)
    QString cmd = _("-break-insert -l -1 ");
    //if (!data->condition.isEmpty())
    //    cmd += "-c " + data->condition + " ";
    cmd += where;
#else
    QString cmd = _("-break-insert -f ");
    //if (!data->condition.isEmpty())
    //    cmd += _("-c ") + data->condition + ' ';
    cmd += where;
#endif
    debugMessage(_("Current state: %1").arg(q->status()));
    postCommand(cmd, NeedsStop, CB(handleBreakInsert), index);
}

void GdbEngine::handleBreakList(const GdbResultRecord &record, const QVariant &)
{
    // 45^done,BreakpointTable={nr_rows="2",nr_cols="6",hdr=[
    // {width="3",alignment="-1",col_name="number",colhdr="Num"}, ...
    // body=[bkpt={number="1",type="breakpoint",disp="keep",enabled="y",
    //  addr="0x000000000040109e",func="main",file="app.cpp",
    //  fullname="/home/apoenitz/dev/work/plugintest/app/app.cpp",
    //  line="11",times="1"},
    //  bkpt={number="2",type="breakpoint",disp="keep",enabled="y",
    //  addr="<PENDING>",pending="plugin.cpp:7",times="0"}] ... }

    if (record.resultClass == GdbResultDone) {
        GdbMi table = record.data.findChild("BreakpointTable");
        if (table.isValid())
            handleBreakList(table);
    }
}

void GdbEngine::handleBreakList(const GdbMi &table)
{
    //qDebug() << "GdbEngine::handleOutput: table:"
    //  << table.toString();
    GdbMi body = table.findChild("body");
    //qDebug() << "GdbEngine::handleOutput: body:"
    //  << body.toString();
    QList<GdbMi> bkpts;
    if (body.isValid()) {
        // Non-Mac
        bkpts = body.children();
    } else {
        // Mac
        bkpts = table.children();
        // remove the 'hdr' and artificial items
        //qDebug() << "FOUND" << bkpts.size() << "BREAKPOINTS";
        for (int i = bkpts.size(); --i >= 0; ) {
            int num = bkpts.at(i).findChild("number").data().toInt();
            if (num <= 0) {
                //qDebug() << "REMOVING" << i << bkpts.at(i).toString();
                bkpts.removeAt(i);
            }
        }
        //qDebug() << "LEFT" << bkpts.size() << "BREAKPOINTS";
    }

    BreakHandler *handler = qq->breakHandler();
    for (int index = 0; index != bkpts.size(); ++index) {
        BreakpointData temp(handler);
        breakpointDataFromOutput(&temp, bkpts.at(index));
        int found = handler->findBreakpoint(temp);
        if (found != -1)
            breakpointDataFromOutput(handler->at(found), bkpts.at(index));
        //else
            //qDebug() << "CANNOT HANDLE RESPONSE" << bkpts.at(index).toString();
    }

    attemptBreakpointSynchronization();
    handler->updateMarkers();
}


void GdbEngine::handleBreakIgnore(const GdbResultRecord &record, const QVariant &cookie)
{
    int index = cookie.toInt();
    // gdb 6.8:
    // ignore 2 0:
    // ~"Will stop next time breakpoint 2 is reached.\n"
    // 28^done
    // ignore 2 12:
    // &"ignore 2 12\n"
    // ~"Will ignore next 12 crossings of breakpoint 2.\n"
    // 29^done
    //
    // gdb 6.3 does not produce any console output
    BreakHandler *handler = qq->breakHandler();
    if (record.resultClass == GdbResultDone && index < handler->size()) {
        QString msg = _(record.data.findChild("consolestreamoutput").data());
        BreakpointData *data = handler->at(index);
        //if (msg.contains(__("Will stop next time breakpoint"))) {
        //    data->bpIgnoreCount = _("0");
        //} else if (msg.contains(__("Will ignore next"))) {
        //    data->bpIgnoreCount = data->ignoreCount;
        //}
        // FIXME: this assumes it is doing the right thing...
        data->bpIgnoreCount = data->ignoreCount;
        attemptBreakpointSynchronization();
        handler->updateMarkers();
    }
}

void GdbEngine::handleBreakCondition(const GdbResultRecord &record, const QVariant &cookie)
{
    int index = cookie.toInt();
    BreakHandler *handler = qq->breakHandler();
    if (record.resultClass == GdbResultDone) {
        // we just assume it was successful. otherwise we had to parse
        // the output stream data
        BreakpointData *data = handler->at(index);
        //qDebug() << "HANDLE BREAK CONDITION" << index << data->condition;
        data->bpCondition = data->condition;
        attemptBreakpointSynchronization();
        handler->updateMarkers();
    } else if (record.resultClass == GdbResultError) {
        QByteArray msg = record.data.findChild("msg").data();
        // happens on Mac
        if (1 || msg.startsWith("Error parsing breakpoint condition. "
                " Will try again when we hit the breakpoint.")) {
            BreakpointData *data = handler->at(index);
            //qDebug() << "ERROR BREAK CONDITION" << index << data->condition;
            data->bpCondition = data->condition;
            attemptBreakpointSynchronization();
            handler->updateMarkers();
        }
    }
}

void GdbEngine::handleBreakInsert(const GdbResultRecord &record, const QVariant &cookie)
{
    int index = cookie.toInt();
    BreakHandler *handler = qq->breakHandler();
    if (record.resultClass == GdbResultDone) {
        //qDebug() << "HANDLE BREAK INSERT" << index;
//#if defined(Q_OS_MAC)
        // interesting only on Mac?
        BreakpointData *data = handler->at(index);
        GdbMi bkpt = record.data.findChild("bkpt");
        //qDebug() << "BKPT:" << bkpt.toString() << " DATA:" << data->toToolTip();
        breakpointDataFromOutput(data, bkpt);
//#endif
        attemptBreakpointSynchronization();
        handler->updateMarkers();
    } else if (record.resultClass == GdbResultError) {
        const BreakpointData *data = handler->at(index);
        // Note that it is perfectly correct that the file name is put
        // in quotes but not escaped. GDB simply is like that.
#if defined(Q_OS_WIN)
        QFileInfo fi(data->fileName);
        QString where = _c('"') + fi.fileName() + _("\":")
            + data->lineNumber;
        //QString where = m_data->fileName + _c(':') + data->lineNumber;
#elif defined(Q_OS_MAC)
        QFileInfo fi(data->fileName);
        QString where = _c('"') + fi.fileName() + _("\":")
            + data->lineNumber;
#else
        //QString where = "\"\\\"" + data->fileName + "\\\":"
        //    + data->lineNumber + "\"";
        QString where = _c('"') + data->fileName + _("\":")
            + data->lineNumber;
        // Should not happen with -break-insert -f. gdb older than 6.8?
        QTC_ASSERT(false, /**/);
#endif
        postCommand(_("break ") + where, CB(handleBreakInsert1), index);
    }
}

void GdbEngine::extractDataFromInfoBreak(const QString &output, BreakpointData *data)
{
    data->bpFileName = _("<MULTIPLE>");

    //qDebug() << output;
    if (output.isEmpty())
        return;
    // "Num     Type           Disp Enb Address            What
    // 4       breakpoint     keep y   <MULTIPLE>         0x00000000004066ad
    // 4.1                         y     0x00000000004066ad in CTorTester
    //  at /data5/dev/ide/main/tests/manual/gdbdebugger/simple/app.cpp:124
    // - or -
    // everything on a single line on Windows for constructors of classes
    // within namespaces.
    // Sometimes the path is relative too.

    // 2    breakpoint     keep y   <MULTIPLE> 0x0040168e
    // 2.1    y     0x0040168e in MainWindow::MainWindow(QWidget*) at mainwindow.cpp:7
    // 2.2    y     0x00401792 in MainWindow::MainWindow(QWidget*) at mainwindow.cpp:7

    // tested in ../../../tests/auto/debugger/
    QRegExp re(_("MULTIPLE.*(0x[0-9a-f]+) in (.*)\\s+at (.*):([\\d]+)([^\\d]|$)"));
    re.setMinimal(true);

    if (re.indexIn(output) != -1) {
        data->bpAddress = re.cap(1);
        data->bpFuncName = re.cap(2).trimmed();
        data->bpLineNumber = re.cap(4);
        QString full = fullName(re.cap(3));
        if (full.isEmpty()) {
            qDebug() << "NO FULL NAME KNOWN FOR" << re.cap(3);
            full = re.cap(3); // FIXME: wrong, but prevents recursion
        }
        data->markerLineNumber = data->bpLineNumber.toInt();
        data->markerFileName = full;
        data->bpFileName = full;
        //qDebug() << "FOUND BREAKPOINT\n" << output
        //    << re.cap(1) << "\n" << re.cap(2) << "\n"
        //    << re.cap(3) << "\n" << re.cap(4) << "\n";
    } else {
        qDebug() << "COULD NOT MATCH " << re.pattern() << " AND " << output;
        data->bpNumber = _("<unavailable>");
    }
}

void GdbEngine::handleBreakInfo(const GdbResultRecord &record, const QVariant &cookie)
{
    int bpNumber = cookie.toInt();
    BreakHandler *handler = qq->breakHandler();
    if (record.resultClass == GdbResultDone) {
        // Old-style output for multiple breakpoints, presumably in a
        // constructor
        int found = handler->findBreakpoint(bpNumber);
        if (found != -1) {
            QString str = QString::fromLocal8Bit(record.data.findChild("consolestreamoutput").data());
            extractDataFromInfoBreak(str, handler->at(found));
            handler->updateMarkers();
            attemptBreakpointSynchronization(); // trigger "ready"
        }
    }
}

void GdbEngine::handleBreakInsert1(const GdbResultRecord &record, const QVariant &cookie)
{
    int index = cookie.toInt();
    BreakHandler *handler = qq->breakHandler();
    if (record.resultClass == GdbResultDone) {
        // Pending breakpoints in dylibs on Mac only?
        BreakpointData *data = handler->at(index);
        GdbMi bkpt = record.data.findChild("bkpt");
        breakpointDataFromOutput(data, bkpt);
        attemptBreakpointSynchronization(); // trigger "ready"
        handler->updateMarkers();
    } else if (record.resultClass == GdbResultError) {
        qDebug() << "INSERTING BREAKPOINT WITH BASE NAME FAILED. GIVING UP";
        BreakpointData *data = handler->at(index);
        data->bpNumber = _("<unavailable>");
        attemptBreakpointSynchronization(); // trigger "ready"
        handler->updateMarkers();
    }
}

void GdbEngine::attemptBreakpointSynchronization()
{
    // Non-lethal check for nested calls
    static bool inBreakpointSychronization = false;
    QTC_ASSERT(!inBreakpointSychronization, /**/);
    inBreakpointSychronization = true;

    BreakHandler *handler = qq->breakHandler();

    foreach (BreakpointData *data, handler->takeDisabledBreakpoints()) {
        QString bpNumber = data->bpNumber;
        if (!bpNumber.trimmed().isEmpty()) {
            postCommand(_("-break-disable ") + bpNumber, NeedsStop);
            data->bpEnabled = false;
        }
    }

    foreach (BreakpointData *data, handler->takeEnabledBreakpoints()) {
        QString bpNumber = data->bpNumber;
        if (!bpNumber.trimmed().isEmpty()) {
            postCommand(_("-break-enable ") + bpNumber, NeedsStop);
            data->bpEnabled = true;
        }
    }

    foreach (BreakpointData *data, handler->takeRemovedBreakpoints()) {
        QString bpNumber = data->bpNumber;
        debugMessage(_("DELETING BP %1 IN %2").arg(bpNumber)
            .arg(data->markerFileName));
        if (!bpNumber.trimmed().isEmpty())
            postCommand(_("-break-delete ") + bpNumber, NeedsStop);
        delete data;
    }

    bool updateNeeded = false;

    for (int index = 0; index != handler->size(); ++index) {
        BreakpointData *data = handler->at(index);
        // multiple breakpoints?
        if (data->bpMultiple && data->bpFileName.isEmpty()) {
            postCommand(_("info break %1").arg(data->bpNumber),
                CB(handleBreakInfo), data->bpNumber.toInt());
            updateNeeded = true;
            break;
        }
    }

    for (int index = 0; index != handler->size(); ++index) {
        BreakpointData *data = handler->at(index);
        // unset breakpoints?
        if (data->bpNumber.isEmpty()) {
            data->bpNumber = _(" ");
            sendInsertBreakpoint(index);
            //qDebug() << "UPDATE NEEDED BECAUSE OF UNKNOWN BREAKPOINT";
            updateNeeded = true;
            break;
        }
    }

    if (!updateNeeded) {
        for (int index = 0; index != handler->size(); ++index) {
            BreakpointData *data = handler->at(index);
            // update conditions if needed
            if (data->bpNumber.toInt() && data->condition != data->bpCondition
                   && !data->conditionsMatch()) {
                postCommand(_("condition %1 %2").arg(data->bpNumber).arg(data->condition),
                            CB(handleBreakCondition), index);
                //qDebug() << "UPDATE NEEDED BECAUSE OF CONDITION"
                //    << data->condition << data->bpCondition;
                updateNeeded = true;
                break;
            }
            // update ignorecount if needed
            if (data->bpNumber.toInt() && data->ignoreCount != data->bpIgnoreCount) {
                postCommand(_("ignore %1 %2").arg(data->bpNumber).arg(data->ignoreCount),
                            CB(handleBreakIgnore), index);
                updateNeeded = true;
                break;
            }
            if (data->bpNumber.toInt() && !data->enabled && data->bpEnabled) {
                postCommand(_("-break-disable ") + data->bpNumber, NeedsStop);
                data->bpEnabled = false;
                updateNeeded = true;
                break;
            }
        }
    }

    for (int index = 0; index != handler->size(); ++index) {
        // happens sometimes on Mac. Brush over symptoms
        BreakpointData *data = handler->at(index);
        if (data->markerFileName.startsWith(__("../"))) {
            data->markerFileName = fullName(data->markerFileName);
            handler->updateMarkers();
        }
    }

    inBreakpointSychronization = false;
}


//////////////////////////////////////////////////////////////////////
//
// Disassembler specific stuff
//
//////////////////////////////////////////////////////////////////////

void GdbEngine::reloadDisassembler()
{
    emit postCommand(_("disassemble"), CB(handleDisassemblerList), m_address);
}

void GdbEngine::handleDisassemblerList(const GdbResultRecord &record,
    const QVariant &cookie)
{
    QString listedLine = cookie.toString();
    QList<DisassemblerLine> lines;
    static const QString pad = _("    ");
    int currentLine = -1;
    if (record.resultClass == GdbResultDone) {
        QString res = _(record.data.findChild("consolestreamoutput").data());
        QTextStream ts(&res, QIODevice::ReadOnly);
        while (!ts.atEnd()) {
            //0x0000000000405fd8 <_ZN11QTextStreamD1Ev@plt+0>:
            //    jmpq   *2151890(%rip)    # 0x6135b0 <_GLOBAL_OFFSET_TABLE_+640>
            //0x0000000000405fde <_ZN11QTextStreamD1Ev@plt+6>:
            //    pushq  $0x4d
            //0x0000000000405fe3 <_ZN11QTextStreamD1Ev@plt+11>:
            //    jmpq   0x405af8 <_init+24>
            //0x0000000000405fe8 <_ZN9QHashData6rehashEi@plt+0>:
            //    jmpq   *2151882(%rip)    # 0x6135b8 <_GLOBAL_OFFSET_TABLE_+648>
            QString str = ts.readLine();
            if (!str.startsWith(__("0x"))) {
                //qDebug() << "IGNORING DISASSEMBLER" << str;
                continue;
            }
            DisassemblerLine line;
            QTextStream ts(&str, QIODevice::ReadOnly);
            ts >> line.address >> line.symbol;
            line.mnemonic = ts.readLine().trimmed();
            if (line.symbol.endsWith(_c(':')))
                line.symbol.chop(1);
            line.addressDisplay = line.address + pad;
            if (line.addressDisplay.startsWith(__("0x00000000")))
                line.addressDisplay.replace(2, 8, QString());
            line.symbolDisplay = line.symbol + pad;

            if (line.address == listedLine)
                currentLine = lines.size();

            lines.append(line);
        }
    } else {
        DisassemblerLine line;
        line.addressDisplay = tr("<could not retreive module information>");
        lines.append(line);
    }

    qq->disassemblerHandler()->setLines(lines);
    if (currentLine != -1)
        qq->disassemblerHandler()->setCurrentLine(currentLine);
}


//////////////////////////////////////////////////////////////////////
//
// Modules specific stuff
//
//////////////////////////////////////////////////////////////////////

void GdbEngine::loadSymbols(const QString &moduleName)
{
    // FIXME: gdb does not understand quoted names here (tested with 6.8)
    postCommand(_("sharedlibrary ") + dotEscape(moduleName));
    reloadModules();
}

void GdbEngine::loadAllSymbols()
{
    postCommand(_("sharedlibrary .*"));
    reloadModules();
}

QList<Symbol> GdbEngine::moduleSymbols(const QString &moduleName)
{
    QList<Symbol> rc;
    bool success = false;
    QString errorMessage;
    do {
        const QString nmBinary = _("nm");
        QProcess proc;
        proc.start(nmBinary, QStringList() << _("-D") << moduleName);
        if (!proc.waitForFinished()) {
            errorMessage = tr("Unable to run '%1': %2").arg(nmBinary, proc.errorString());
            break;
        }
        const QString contents = QString::fromLocal8Bit(proc.readAllStandardOutput());
        const QRegExp re(_("([0-9a-f]+)?\\s+([^\\s]+)\\s+([^\\s]+)"));
        Q_ASSERT(re.isValid());
        foreach (const QString &line, contents.split(_c('\n'))) {
            if (re.indexIn(line) != -1) {
                Symbol symbol;
                symbol.address = re.cap(1);
                symbol.state = re.cap(2);
                symbol.name = re.cap(3);
                rc.push_back(symbol);
            } else {
                qWarning("moduleSymbols: unhandled: %s", qPrintable(line));
            }
        }
        success = true;
    } while (false);
    if (!success)
        qWarning("moduleSymbols: %s\n", qPrintable(errorMessage));
    return rc;
}

void GdbEngine::reloadModules()
{
    postCommand(_("info shared"), CB(handleModulesList));
}

void GdbEngine::handleModulesList(const GdbResultRecord &record, const QVariant &)
{
    QList<Module> modules;
    if (record.resultClass == GdbResultDone) {
        // that's console-based output, likely Linux or Windows,
        // but we can avoid the #ifdef here
        QString data = QString::fromLocal8Bit(record.data.findChild("consolestreamoutput").data());
        QTextStream ts(&data, QIODevice::ReadOnly);
        while (!ts.atEnd()) {
            QString line = ts.readLine();
            if (!line.startsWith(__("0x")))
                continue;
            Module module;
            QString symbolsRead;
            QTextStream ts(&line, QIODevice::ReadOnly);
            ts >> module.startAddress >> module.endAddress >> symbolsRead;
            module.moduleName = ts.readLine().trimmed();
            module.symbolsRead = (symbolsRead == __("Yes"));
            modules.append(module);
        }
        if (modules.isEmpty()) {
            // Mac has^done,shlib-info={num="1",name="dyld",kind="-",
            // dyld-addr="0x8fe00000",reason="dyld",requested-state="Y",
            // state="Y",path="/usr/lib/dyld",description="/usr/lib/dyld",
            // loaded_addr="0x8fe00000",slide="0x0",prefix="__dyld_"},
            // shlib-info={...}...
            foreach (const GdbMi &item, record.data.children()) {
                Module module;
                module.moduleName = QString::fromLocal8Bit(item.findChild("path").data());
                module.symbolsRead = (item.findChild("state").data() == "Y");
                module.startAddress = _(item.findChild("loaded_addr").data());
                //: End address of loaded module
                module.endAddress = tr("<unknown>");
                modules.append(module);
            }
        }
    }
    qq->modulesHandler()->setModules(modules);
}


//////////////////////////////////////////////////////////////////////
//
// Source files specific stuff
//
//////////////////////////////////////////////////////////////////////

void GdbEngine::reloadSourceFiles()
{
    postCommand(_("-file-list-exec-source-files"), CB(handleQuerySources));
}


//////////////////////////////////////////////////////////////////////
//
// Stack specific stuff
//
//////////////////////////////////////////////////////////////////////

void GdbEngine::handleStackSelectThread(const GdbResultRecord &, const QVariant &)
{
    //qDebug("FIXME: StackHandler::handleOutput: SelectThread");
    q->showStatusMessage(tr("Retrieving data for stack view..."), 3000);
    reloadStack();
}


void GdbEngine::handleStackListFrames(const GdbResultRecord &record, const QVariant &cookie)
{
    bool isFull = cookie.toBool();
    QList<StackFrame> stackFrames;

    const GdbMi stack = record.data.findChild("stack");
    stack.toString();
    if (!stack.isValid()) {
        qDebug() << "FIXME: stack:" << stack.toString();
        return;
    }

    int topFrame = -1;

    int n = stack.childCount();
    for (int i = 0; i != n; ++i) {
        //qDebug() << "HANDLING FRAME:" << stack.childAt(i).toString();
        const GdbMi frameMi = stack.childAt(i);
        StackFrame frame(i);
        QStringList files;
        files.append(QString::fromLocal8Bit(frameMi.findChild("fullname").data()));
        files.append(QString::fromLocal8Bit(frameMi.findChild("file").data()));
        frame.file = fullName(files);
        frame.function = _(frameMi.findChild("func").data());
        frame.from = _(frameMi.findChild("from").data());
        frame.line = frameMi.findChild("line").data().toInt();
        frame.address = _(frameMi.findChild("addr").data());

        stackFrames.append(frame);

#if defined(Q_OS_WIN)
        const bool isBogus =
            // Assume this is wrong and points to some strange stl_algobase
            // implementation. Happens on Karsten's XP system with Gdb 5.50
            (frame.file.endsWith(__("/bits/stl_algobase.h")) && frame.line == 150)
            // Also wrong. Happens on Vista with Gdb 5.50
               || (frame.function == __("operator new") && frame.line == 151);

        // immediately leave bogus frames
        if (topFrame == -1 && isBogus) {
            postCommand(_("-exec-finish"));
            return;
        }

#endif

        // Initialize top frame to the first valid frame
        const bool isValid = !frame.file.isEmpty() && !frame.function.isEmpty();
        if (isValid && topFrame == -1)
            topFrame = i;
    }

    bool canExpand = !isFull 
        && (n >= theDebuggerAction(MaximalStackDepth)->value().toInt());
    theDebuggerAction(ExpandStack)->setEnabled(canExpand);
    qq->stackHandler()->setFrames(stackFrames, canExpand);

    if (topFrame != -1) {
        // updates of locals already triggered early
        const StackFrame &frame = qq->stackHandler()->currentFrame();
        if (frame.isUsable())
            q->gotoLocation(frame.file, frame.line, true);
        else
            qDebug() << "FULL NAME NOT USABLE 0:" << frame.file << topFrame;
    }
}

void GdbEngine::selectThread(int index)
{
    //reset location arrow
    q->resetLocation();

    ThreadsHandler *threadsHandler = qq->threadsHandler();
    threadsHandler->setCurrentThread(index);

    QList<ThreadData> threads = threadsHandler->threads();
    QTC_ASSERT(index < threads.size(), return);
    int id = threads.at(index).id;
    q->showStatusMessage(tr("Retrieving data for stack view..."), 10000);
    postCommand(_("-thread-select %1").arg(id), CB(handleStackSelectThread));
}

void GdbEngine::activateFrame(int frameIndex)
{
    if (q->status() != DebuggerInferiorStopped)
        return;

    StackHandler *stackHandler = qq->stackHandler();
    int oldIndex = stackHandler->currentIndex();
    //qDebug() << "ACTIVATE FRAME:" << frameIndex << oldIndex
    //    << stackHandler->currentIndex();

    if (frameIndex == stackHandler->stackSize()) {
        reloadFullStack();
        return;
    }
    QTC_ASSERT(frameIndex < stackHandler->stackSize(), return);

    if (oldIndex != frameIndex) {
        setTokenBarrier();

        // Assuming this always succeeds saves a roundtrip.
        // Otherwise the lines below would need to get triggered
        // after a response to this -stack-select-frame here.
        postCommand(_("-stack-select-frame ") + QString::number(frameIndex));

        stackHandler->setCurrentIndex(frameIndex);
        updateLocals();
    }

    const StackFrame &frame = stackHandler->currentFrame();

    if (frame.isUsable())
        q->gotoLocation(frame.file, frame.line, true);
    else
        qDebug() << "FULL NAME NOT USABLE:" << frame.file;
}

void GdbEngine::handleStackListThreads(const GdbResultRecord &record, const QVariant &cookie)
{
    int id = cookie.toInt();
    // "72^done,{thread-ids={thread-id="2",thread-id="1"},number-of-threads="2"}
    const QList<GdbMi> items = record.data.findChild("thread-ids").children();
    QList<ThreadData> threads;
    int currentIndex = -1;
    for (int index = 0, n = items.size(); index != n; ++index) {
        ThreadData thread;
        thread.id = items.at(index).data().toInt();
        threads.append(thread);
        if (thread.id == id) {
            //qDebug() << "SETTING INDEX TO:" << index << " ID:" << id << " RECOD:" << record.toString();
            currentIndex = index;
        }
    }
    ThreadsHandler *threadsHandler = qq->threadsHandler();
    threadsHandler->setThreads(threads);
    threadsHandler->setCurrentThread(currentIndex);
}


//////////////////////////////////////////////////////////////////////
//
// Register specific stuff
//
//////////////////////////////////////////////////////////////////////

static inline char registerFormatChar()
{
    switch(checkedRegisterFormatAction()) {
    case FormatHexadecimal:
        return 'x';
    case FormatDecimal:
        return 'd';
    case FormatOctal:
        return 'o';
    case FormatBinary:
        return 't';
    case FormatRaw:
        return 'r';
    default:
        break;
    }
    return 'N';
}

void GdbEngine::reloadRegisters()
{
    postCommand(_("-data-list-register-values ") + _c(registerFormatChar()),
                Discardable, CB(handleRegisterListValues));
}

void GdbEngine::handleRegisterListNames(const GdbResultRecord &record, const QVariant &)
{
    if (record.resultClass != GdbResultDone)
        return;

    QList<Register> registers;
    foreach (const GdbMi &item, record.data.findChild("register-names").children())
        registers.append(Register(_(item.data())));

    qq->registerHandler()->setRegisters(registers);
}

void GdbEngine::handleRegisterListValues(const GdbResultRecord &record, const QVariant &)
{
    if (record.resultClass != GdbResultDone)
        return;

    QList<Register> registers = qq->registerHandler()->registers();

    // 24^done,register-values=[{number="0",value="0xf423f"},...]
    foreach (const GdbMi &item, record.data.findChild("register-values").children()) {
        int index = item.findChild("number").data().toInt();
        if (index < registers.size()) {
            Register &reg = registers[index];
            QString value = _(item.findChild("value").data());
            reg.changed = (value != reg.value);
            if (reg.changed)
                reg.value = value;
        }
    }
    qq->registerHandler()->setRegisters(registers);
}


//////////////////////////////////////////////////////////////////////
//
// Thread specific stuff
//
//////////////////////////////////////////////////////////////////////

bool GdbEngine::supportsThreads() const
{
    // 6.3 crashes happily on -thread-list-ids. So don't use it.
    // The test below is a semi-random pick, 6.8 works fine
    return m_gdbVersion > 60500;
}

//////////////////////////////////////////////////////////////////////
//
// Tooltip specific stuff
//
//////////////////////////////////////////////////////////////////////

static WatchData m_toolTip;
static QString m_toolTipExpression;
static QPoint m_toolTipPos;
static QMap<QString, WatchData> m_toolTipCache;

void GdbEngine::setToolTipExpression(const QPoint &mousePos, TextEditor::ITextEditor *editor, int cursorPos)
{
    if (q->status() != DebuggerInferiorStopped || !isCppEditor(editor)) {
        //qDebug() << "SUPPRESSING DEBUGGER TOOLTIP, INFERIOR NOT STOPPED/Non Cpp editor";
        return;
    }

    if (theDebuggerBoolSetting(DebugDebuggingHelpers)) {
        // minimize interference
        return;
    }

    m_toolTipPos = mousePos;
    int line, column;
    m_toolTipExpression = cppExpressionAt(editor, cursorPos, &line, &column);
    QString exp = m_toolTipExpression;
/*
    if (m_toolTip.isTypePending()) {
        qDebug() << "suppressing duplicated tooltip creation";
        return;
    }
*/
    if (m_toolTipCache.contains(exp)) {
        const WatchData & data = m_toolTipCache[exp];
        // FIXME: qq->watchHandler()->collapseChildren(data.iname);
        insertData(data);
        return;
    }

    QToolTip::hideText();
    if (exp.isEmpty() || exp.startsWith(_c('#')))  {
        QToolTip::hideText();
        return;
    }

    if (!hasLetterOrNumber(exp)) {
        QToolTip::showText(m_toolTipPos,
            tr("'%1' contains no identifier").arg(exp));
        return;
    }

    if (isKeyWord(exp))
        return;

    if (exp.startsWith(_c('"')) && exp.endsWith(_c('"')))  {
        QToolTip::showText(m_toolTipPos, tr("String literal %1").arg(exp));
        return;
    }

    if (exp.startsWith(__("++")) || exp.startsWith(__("--")))
        exp = exp.mid(2);

    if (exp.endsWith(__("++")) || exp.endsWith(__("--")))
        exp = exp.mid(2);

    if (exp.startsWith(_c('<')) || exp.startsWith(_c('[')))
        return;

    if (hasSideEffects(exp)) {
        QToolTip::showText(m_toolTipPos,
            tr("Cowardly refusing to evaluate expression '%1' "
               "with potential side effects").arg(exp));
        return;
    }

    // Gdb crashes when creating a variable object with the name
    // of the type of 'this'
/*
    for (int i = 0; i != m_currentLocals.childCount(); ++i) {
        if (m_currentLocals.childAt(i).exp == "this") {
            qDebug() << "THIS IN ROW" << i;
            if (m_currentLocals.childAt(i).type.startsWith(exp)) {
                QToolTip::showText(m_toolTipPos,
                    tr("%1: type of current 'this'").arg(exp));
                qDebug() << " TOOLTIP CRASH SUPPRESSED";
                return;
            }
            break;
        }
    }
*/

    //if (m_manager->status() != DebuggerInferiorStopped)
    //    return;

    // FIXME: 'exp' can contain illegal characters
    m_toolTip = WatchData();
    //m_toolTip.level = 0;
   // m_toolTip.row = 0;
   // m_toolTip.parentIndex = 2;
    m_toolTip.exp = exp;
    m_toolTip.name = exp;
    m_toolTip.iname = tooltipIName;
    insertData(m_toolTip);
    updateWatchModel2();
}


//////////////////////////////////////////////////////////////////////
//
// Watch specific stuff
//
//////////////////////////////////////////////////////////////////////

//: Variable
static const QString strNotInScope =
        QApplication::translate("Debugger::Internal::GdbEngine", "<not in scope>");


static void setWatchDataValue(WatchData &data, const GdbMi &mi,
    int encoding = 0)
{
    if (mi.isValid())
        data.setValue(decodeData(mi.data(), encoding));
    else
        data.setValueNeeded();
}

static void setWatchDataEditValue(WatchData &data, const GdbMi &mi)
{
    if (mi.isValid())
        data.editvalue = mi.data();
}

static void setWatchDataValueToolTip(WatchData &data, const GdbMi &mi,
        int encoding = 0)
{
    if (mi.isValid())
        data.setValueToolTip(decodeData(mi.data(), encoding));
}

static void setWatchDataChildCount(WatchData &data, const GdbMi &mi)
{
    if (mi.isValid()) {
        data.childCount = mi.data().toInt();
        data.setChildCountUnneeded();
        if (data.childCount == 0)
            data.setChildrenUnneeded();
    } else {
        data.childCount = -1;
    }
}

static void setWatchDataValueDisabled(WatchData &data, const GdbMi &mi)
{
    if (mi.data() == "true")
        data.valuedisabled = true;
    else if (mi.data() == "false")
        data.valuedisabled = false;
}

static void setWatchDataExpression(WatchData &data, const GdbMi &mi)
{
    if (mi.isValid())
        data.exp = _('(' + mi.data() + ')');
}

static void setWatchDataAddress(WatchData &data, const GdbMi &mi)
{
    if (mi.isValid()) {
        data.addr = _(mi.data());
        if (data.exp.isEmpty() && !data.addr.startsWith(_("$")))
            data.exp = _("(*(") + gdbQuoteTypes(data.type) + _("*)") + data.addr + _c(')');
    }
}

static void setWatchDataSAddress(WatchData &data, const GdbMi &mi)
{
    if (mi.isValid())
        data.saddr = _(mi.data());
}

void GdbEngine::setUseDebuggingHelpers(const QVariant &on)
{
    //qDebug() << "SWITCHING ON/OFF DUMPER DEBUGGING:" << on;
    // FIXME: a bit too harsh, but otherwise the treeview sometimes look funny
    //m_expandedINames.clear();
    setTokenBarrier();
    updateLocals();
}

bool GdbEngine::hasDebuggingHelperForType(const QString &type) const
{
    if (!theDebuggerBoolSetting(UseDebuggingHelpers))
        return false;

    if (!startModeAllowsDumpers()) {
        // "call" is not possible in gdb when looking at core files
        return type == __("QString") || type.endsWith(__("::QString"))
            || type == __("QStringList") || type.endsWith(__("::QStringList"));
    }

    if (theDebuggerBoolSetting(DebugDebuggingHelpers)
            && qq->stackHandler()->isDebuggingDebuggingHelpers())
        return false;

    if (m_debuggingHelperState != DebuggingHelperAvailable)
        return false;

    // simple types
    return m_dumperHelper.type(type) != QtDumperHelper::UnknownType;
}

static inline QString msgRetrievingWatchData(int pending)
{
    return GdbEngine::tr("Retrieving data for watch view (%n requests pending)...", 0, pending);
}

void GdbEngine::runDirectDebuggingHelper(const WatchData &data, bool dumpChildren)
{
    Q_UNUSED(dumpChildren);
    QString type = data.type;
    QString cmd;

    if (type == __("QString") || type.endsWith(__("::QString")))
        cmd = _("qdumpqstring (&") + data.exp + _c(')');
    else if (type == __("QStringList") || type.endsWith(__("::QStringList")))
        cmd = _("qdumpqstringlist (&") + data.exp + _c(')');

    QVariant var;
    var.setValue(data);
    postCommand(cmd, WatchUpdate, CB(handleDebuggingHelperValue3), var);

    q->showStatusMessage(msgRetrievingWatchData(m_pendingRequests + 1), 10000);
}

void GdbEngine::runDebuggingHelper(const WatchData &data0, bool dumpChildren)
{
    if (!startModeAllowsDumpers()) {
        runDirectDebuggingHelper(data0, dumpChildren);
        return;
    }
    WatchData data = data0;

    QByteArray params;
    QStringList extraArgs;
    const QtDumperHelper::TypeData td = m_dumperHelper.typeData(data0.type);
    m_dumperHelper.evaluationParameters(data, td, QtDumperHelper::GdbDebugger, &params, &extraArgs);

    //int protocol = (data.iname.startsWith("watch") && data.type == "QImage") ? 3 : 2;
    //int protocol = data.iname.startsWith("watch") ? 3 : 2;
    const int protocol = 2;
    //int protocol = isDisplayedIName(data.iname) ? 3 : 2;

    QString addr;
    if (data.addr.startsWith(__("0x")))
        addr = _("(void*)") + data.addr;
    else if (data.exp.isEmpty()) // happens e.g. for QAbstractItem
        addr = _("0");
    else
        addr = _("&(") + data.exp + _c(')');

    sendWatchParameters(params);

    QString cmd;
    QTextStream(&cmd) << "call " << "(void*)qDumpObjectData440(" <<
            protocol << ',' << "%1+1"                // placeholder for token
            <<',' <<  addr << ',' << (dumpChildren ? "1" : "0")
            << ',' << extraArgs.join(QString(_c(','))) <<  ')';

    QVariant var;
    var.setValue(data);
    postCommand(cmd, WatchUpdate | EmbedToken, CB(handleDebuggingHelperValue1), var);

    q->showStatusMessage(msgRetrievingWatchData(m_pendingRequests + 1), 10000);

    // retrieve response
    postCommand(_("p (char*)&qDumpOutBuffer"), WatchUpdate,
        CB(handleDebuggingHelperValue2), var);
}

void GdbEngine::createGdbVariable(const WatchData &data)
{
    postCommand(_("-var-delete \"%1\"").arg(data.iname), WatchUpdate);
    QString exp = data.exp;
    if (exp.isEmpty() && data.addr.startsWith(__("0x")))
        exp = _("*(") + gdbQuoteTypes(data.type) + _("*)") + data.addr;
    QVariant val = QVariant::fromValue<WatchData>(data);
    postCommand(_("-var-create \"%1\" * \"%2\"").arg(data.iname).arg(exp),
        WatchUpdate, CB(handleVarCreate), val);
}

void GdbEngine::updateSubItem(const WatchData &data0)
{
    WatchData data = data0;
    #if DEBUG_SUBITEM
    qDebug() << "UPDATE SUBITEM:" << data.toString();
    #endif
    QTC_ASSERT(data.isValid(), return);

    // in any case we need the type first
    if (data.isTypeNeeded()) {
        // This should only happen if we don't have a variable yet.
        // Let's play safe, though.
        if (!data.variable.isEmpty()) {
            // Update: It does so for out-of-scope watchers.
            #if 1
            qDebug() << "FIXME: GdbEngine::updateSubItem:"
                 << data.toString() << "should not happen";
            #else
            data.setType(strNotInScope);
            data.setValue(strNotInScope);
            data.setChildCount(0);
            insertData(data);
            return;
            #endif
        }
        // The WatchVarCreate handler will receive type information
        // and re-insert a WatchData item with correct type, so
        // we will not re-enter this bit.
        // FIXME: Concurrency issues?
        createGdbVariable(data);
        return;
    }

    // we should have a type now. this is relied upon further below
    QTC_ASSERT(!data.type.isEmpty(), return);

    // a common case that can be easily solved
    if (data.isChildrenNeeded() && isPointerType(data.type)
        && !hasDebuggingHelperForType(data.type)) {
        // We sometimes know what kind of children pointers have
        #if DEBUG_SUBITEM
        qDebug() << "IT'S A POINTER";
        #endif
#if 1
        insertData(data.pointerChildPlaceHolder());
        data.setChildrenUnneeded();
        insertData(data);
#else
        // Try automatic dereferentiation
        data.exp = "*(" + data.exp + ")";
        data.type = data.type + "."; // FIXME: fragile HACK to avoid recursion
        insertData(data);
#endif
        return;
    }

    if (data.isValueNeeded() && hasDebuggingHelperForType(data.type)) {
        #if DEBUG_SUBITEM
        qDebug() << "UPDATE SUBITEM: CUSTOMVALUE";
        #endif
        runDebuggingHelper(data, qq->watchHandler()->isExpandedIName(data.iname));
        return;
    }

/*
    if (data.isValueNeeded() && data.exp.isEmpty()) {
        #if DEBUG_SUBITEM
        qDebug() << "UPDATE SUBITEM: NO EXPRESSION?";
        #endif
        data.setError("<no expression given>");
        insertData(data);
        return;
    }
*/

    if (data.isValueNeeded() && data.variable.isEmpty()) {
        #if DEBUG_SUBITEM
        qDebug() << "UPDATE SUBITEM: VARIABLE NEEDED FOR VALUE";
        #endif
        createGdbVariable(data);
        // the WatchVarCreate handler will re-insert a WatchData
        // item, with valueNeeded() set.
        return;
    }

    if (data.isValueNeeded()) {
        QTC_ASSERT(!data.variable.isEmpty(), return); // tested above
        #if DEBUG_SUBITEM
        qDebug() << "UPDATE SUBITEM: VALUE";
        #endif
        QString cmd = _("-var-evaluate-expression \"") + data.iname + _c('"');
        postCommand(cmd, WatchUpdate, CB(handleEvaluateExpression),
            QVariant::fromValue(data));
        return;
    }

    if (data.isChildrenNeeded() && hasDebuggingHelperForType(data.type)) {
        #if DEBUG_SUBITEM
        qDebug() << "UPDATE SUBITEM: CUSTOMVALUE WITH CHILDREN";
        #endif
        runDebuggingHelper(data, true);
        return;
    }

    if (data.isChildrenNeeded() && data.variable.isEmpty()) {
        #if DEBUG_SUBITEM
        qDebug() << "UPDATE SUBITEM: VARIABLE NEEDED FOR CHILDREN";
        #endif
        createGdbVariable(data);
        // the WatchVarCreate handler will re-insert a WatchData
        // item, with childrenNeeded() set.
        return;
    }

    if (data.isChildrenNeeded()) {
        QTC_ASSERT(!data.variable.isEmpty(), return); // tested above
        QString cmd = _("-var-list-children --all-values \"") + data.variable + _c('"');
        postCommand(cmd, WatchUpdate, CB(handleVarListChildren), QVariant::fromValue(data));
        return;
    }

    if (data.isChildCountNeeded() && hasDebuggingHelperForType(data.type)) {
        #if DEBUG_SUBITEM
        qDebug() << "UPDATE SUBITEM: CUSTOMVALUE WITH CHILDREN";
        #endif
        runDebuggingHelper(data, qq->watchHandler()->isExpandedIName(data.iname));
        return;
    }

    if (data.isChildCountNeeded() && data.variable.isEmpty()) {
        #if DEBUG_SUBITEM
        qDebug() << "UPDATE SUBITEM: VARIABLE NEEDED FOR CHILDCOUNT";
        #endif
        createGdbVariable(data);
        // the WatchVarCreate handler will re-insert a WatchData
        // item, with childrenNeeded() set.
        return;
    }

    if (data.isChildCountNeeded()) {
        QTC_ASSERT(!data.variable.isEmpty(), return); // tested above
        QString cmd = _("-var-list-children --all-values \"") + data.variable + _c('"');
        postCommand(cmd, Discardable, CB(handleVarListChildren), QVariant::fromValue(data));
        return;
    }

    qDebug() << "FIXME: UPDATE SUBITEM:" << data.toString();
    QTC_ASSERT(false, return);
}

void GdbEngine::updateWatchModel()
{
    m_pendingRequests = 0;
    PENDING_DEBUG("EXTERNAL TRIGGERING UPDATE WATCH MODEL");
    updateWatchModel2();
}

void GdbEngine::updateWatchModel2()
{
    PENDING_DEBUG("UPDATE WATCH MODEL");
    QList<WatchData> incomplete = qq->watchHandler()->takeCurrentIncompletes();
    //QTC_ASSERT(incomplete.isEmpty(), /**/);
    if (!incomplete.isEmpty()) {
        #if DEBUG_PENDING
        qDebug() << "##############################################";
        qDebug() << "UPDATE MODEL, FOUND INCOMPLETES:";
        foreach (const WatchData &data, incomplete)
            qDebug() << data.toString();
        #endif

        // Bump requests to avoid model rebuilding during the nested
        // updateWatchModel runs.
        ++m_pendingRequests;
        foreach (const WatchData &data, incomplete)
            updateSubItem(data);
        PENDING_DEBUG("INTERNAL TRIGGERING UPDATE WATCH MODEL");
        updateWatchModel2();
        --m_pendingRequests;

        return;
    }

    if (m_pendingRequests > 0) {
        PENDING_DEBUG("UPDATE MODEL, PENDING: " << m_pendingRequests);
        return;
    }

    PENDING_DEBUG("REBUILDING MODEL");
    emit gdbInputAvailable(QString(),
        _c('[') + currentTime() + _("]    <Rebuild Watchmodel>"));
    q->showStatusMessage(tr("Finished retrieving data."), 400);
    qq->watchHandler()->rebuildModel();

    if (!m_toolTipExpression.isEmpty()) {
        WatchData *data = qq->watchHandler()->findData(tooltipIName);
        if (data) {
            //m_toolTipCache[data->exp] = *data;
            QToolTip::showText(m_toolTipPos,
                    _c('(') + data->type + _(") ") + data->exp + _(" = ") + data->value);
        } else {
            QToolTip::showText(m_toolTipPos,
                tr("Cannot evaluate expression: %1").arg(m_toolTipExpression));
        }
    }
}

void GdbEngine::handleQueryDebuggingHelper(const GdbResultRecord &record, const QVariant &)
{
    m_dumperHelper.clear();
    //qDebug() << "DATA DUMPER TRIAL:" << record.toString();
    GdbMi output = record.data.findChild("consolestreamoutput");
    QByteArray out = output.data();
    out = out.mid(out.indexOf('"') + 2); // + 1 is success marker
    out = out.left(out.lastIndexOf('"'));
    out.replace('\\', ""); // optimization: dumper output never needs real C unquoting
    out = "dummy={" + out + "}";
    //qDebug() << "OUTPUT:" << out;

    GdbMi contents;
    contents.fromString(out);
    GdbMi simple = contents.findChild("dumpers");

    m_dumperHelper.setQtNamespace(_(contents.findChild("namespace").data()));
    GdbMi qtversion = contents.findChild("qtversion");
    int qtv = 0;
    if (qtversion.children().size() == 3) {
        qtv = (qtversion.childAt(0).data().toInt() << 16)
                    + (qtversion.childAt(1).data().toInt() << 8)
                    + qtversion.childAt(2).data().toInt();
        //qDebug() << "FOUND QT VERSION:" << qtversion.toString() << m_qtVersion;
    }
    m_dumperHelper.setQtVersion(qtv);

    //qDebug() << "CONTENTS:" << contents.toString();
    //qDebug() << "SIMPLE DUMPERS:" << simple.toString();

    QStringList availableSimpleDebuggingHelpers;
    foreach (const GdbMi &item, simple.children())
        availableSimpleDebuggingHelpers.append(_(item.data()));
    m_dumperHelper.parseQueryTypes(availableSimpleDebuggingHelpers, QtDumperHelper::GdbDebugger);

    if (availableSimpleDebuggingHelpers.isEmpty()) {
        if (!m_dumperInjectionLoad) // Retry if thread has not terminated yet.
            m_debuggingHelperState = DebuggingHelperUnavailable;
        q->showStatusMessage(tr("Debugging helpers not found."));
        //QMessageBox::warning(q->mainWindow(),
        //    tr("Cannot find special data dumpers"),
        //    tr("The debugged binary does not contain information needed for "
        //            "nice display of Qt data types.\n\n"
        //            "You might want to try including the file\n\n"
        //            ".../share/qtcreator/gdbmacros/gdbmacros.cpp\n\n"
        //            "into your project directly.")
        //        );
    } else {
        m_debuggingHelperState = DebuggingHelperAvailable;
        q->showStatusMessage(tr("%n custom dumpers found.", 0, m_dumperHelper.typeCount()));
    }
    //qDebug() << m_dumperHelper.toString(true);
    //qDebug() << m_availableSimpleDebuggingHelpers << "DATA DUMPERS AVAILABLE";
}

void GdbEngine::sendWatchParameters(const QByteArray &params0)
{
    QByteArray params = params0;
    params.append('\0');
    char buf[50];
    sprintf(buf, "set {char[%d]} &qDumpInBuffer = {", params.size());
    QByteArray encoded;
    encoded.append(buf);
    for (int i = 0; i != params.size(); ++i) {
        sprintf(buf, "%d,", int(params[i]));
        encoded.append(buf);
    }
    encoded[encoded.size() - 1] = '}';

    params.replace('\0','!');
    emit gdbInputAvailable(QString(), QString::fromUtf8(params));

    postCommand(_(encoded));
}

void GdbEngine::handleVarAssign(const GdbResultRecord &, const QVariant &)
{
    // everything might have changed, force re-evaluation
    // FIXME: Speed this up by re-using variables and only
    // marking values as 'unknown'
    setTokenBarrier();
    updateLocals();
}

void GdbEngine::setWatchDataType(WatchData &data, const GdbMi &mi)
{
    if (mi.isValid()) {
        QString miData = _(mi.data());
        if (!data.framekey.isEmpty())
            m_varToType[data.framekey] = miData;
        data.setType(miData);
    } else if (data.type.isEmpty()) {
        data.setTypeNeeded();
    }
}

void GdbEngine::handleVarCreate(const GdbResultRecord &record,
    const QVariant &cookie)
{
    WatchData data = cookie.value<WatchData>();
    // happens e.g. when we already issued a var-evaluate command
    if (!data.isValid())
        return;
    //qDebug() << "HANDLE VARIABLE CREATION:" << data.toString();
    if (record.resultClass == GdbResultDone) {
        data.variable = data.iname;
        setWatchDataType(data, record.data.findChild("type"));
        if (hasDebuggingHelperForType(data.type)) {
            // we do not trust gdb if we have a custom dumper
            if (record.data.findChild("children").isValid())
                data.setChildrenUnneeded();
            else if (qq->watchHandler()->isExpandedIName(data.iname))
                data.setChildrenNeeded();
            insertData(data);
        } else {
            if (record.data.findChild("children").isValid())
                data.setChildrenUnneeded();
            else if (qq->watchHandler()->isExpandedIName(data.iname))
                data.setChildrenNeeded();
            setWatchDataChildCount(data, record.data.findChild("numchild"));
            //if (data.isValueNeeded() && data.childCount > 0)
            //    data.setValue(QString());
            insertData(data);
        }
    } else if (record.resultClass == GdbResultError) {
        data.setError(QString::fromLocal8Bit(record.data.findChild("msg").data()));
        if (data.isWatcher()) {
            data.value = strNotInScope;
            data.type = _(" ");
            data.setAllUnneeded();
            data.setChildCount(0);
            data.valuedisabled = true;
            insertData(data);
        }
    }
}

void GdbEngine::handleEvaluateExpression(const GdbResultRecord &record,
    const QVariant &cookie)
{
    WatchData data = cookie.value<WatchData>();
    QTC_ASSERT(data.isValid(), qDebug() << "HUH?");
    if (record.resultClass == GdbResultDone) {
        //if (col == 0)
        //    data.name = record.data.findChild("value").data();
        //else
            setWatchDataValue(data, record.data.findChild("value"));
    } else if (record.resultClass == GdbResultError) {
        data.setError(QString::fromLocal8Bit(record.data.findChild("msg").data()));
    }
    //qDebug() << "HANDLE EVALUATE EXPRESSION:" << data.toString();
    insertData(data);
    //updateWatchModel2();
}

void GdbEngine::handleDebuggingHelperSetup(const GdbResultRecord &record, const QVariant &)
{
    //qDebug() << "CUSTOM SETUP RESULT:" << record.toString();
    if (record.resultClass == GdbResultDone) {
    } else if (record.resultClass == GdbResultError) {
        QString msg = QString::fromLocal8Bit(record.data.findChild("msg").data());
        //qDebug() << "CUSTOM DUMPER SETUP ERROR MESSAGE:" << msg;
        q->showStatusMessage(tr("Custom dumper setup: %1").arg(msg), 10000);
    }
}

void GdbEngine::handleDebuggingHelperValue1(const GdbResultRecord &record,
    const QVariant &cookie)
{
    WatchData data = cookie.value<WatchData>();
    QTC_ASSERT(data.isValid(), return);
    if (record.resultClass == GdbResultDone) {
        // ignore this case, data will follow
    } else if (record.resultClass == GdbResultError) {
        // Record an extra result, as the socket result will be lost
        // in transmission
        //--m_pendingRequests;
        QString msg = QString::fromLocal8Bit(record.data.findChild("msg").data());
        //qDebug() << "CUSTOM DUMPER ERROR MESSAGE:" << msg;
#ifdef QT_DEBUG
        // Make debugging of dumpers easier
        if (theDebuggerBoolSetting(DebugDebuggingHelpers)
                && msg.startsWith(__("The program being debugged stopped while"))
                && msg.contains(__("qDumpObjectData440"))) {
            // Fake full stop
            postCommand(_("p 0"), CB(handleAsyncOutput2));  // dummy
            return;
        }
#endif
        //if (msg.startsWith("The program being debugged was sig"))
        //    msg = strNotInScope;
        //if (msg.startsWith("The program being debugged stopped while"))
        //    msg = strNotInScope;
        //data.setError(msg);
        //insertData(data);
    }
}

void GdbEngine::handleDebuggingHelperValue2(const GdbResultRecord &record,
    const QVariant &cookie)
{
    WatchData data = cookie.value<WatchData>();
    QTC_ASSERT(data.isValid(), return);
    //qDebug() << "CUSTOM VALUE RESULT:" << record.toString();
    //qDebug() << "FOR DATA:" << data.toString() << record.resultClass;
    if (record.resultClass != GdbResultDone) {
        qDebug() << "STRANGE CUSTOM DUMPER RESULT DATA:" << data.toString();
        return;
    }

    GdbMi output = record.data.findChild("consolestreamoutput");
    QByteArray out = output.data();

    int markerPos = out.indexOf('"') + 1; // position of 'success marker'
    if (markerPos == 0 || out.at(markerPos) == 'f') {  // 't' or 'f'
        // custom dumper produced no output
        data.setError(strNotInScope);
        insertData(data);
        return;
    }

    out = out.mid(markerPos +  1);
    out = out.left(out.lastIndexOf('"'));
    out.replace('\\', ""); // optimization: dumper output never needs real C unquoting
    out = "dummy={" + out + "}";

    GdbMi contents;
    contents.fromString(out);
    //qDebug() << "CONTENTS" << contents.toString(true);
    if (!contents.isValid()) {
        data.setError(strNotInScope);
        insertData(data);
        return;
    }

    setWatchDataType(data, contents.findChild("type"));
    setWatchDataValue(data, contents.findChild("value"),
        contents.findChild("valueencoded").data().toInt());
    setWatchDataAddress(data, contents.findChild("addr"));
    setWatchDataSAddress(data, contents.findChild("saddr"));
    setWatchDataChildCount(data, contents.findChild("numchild"));
    setWatchDataValueToolTip(data, contents.findChild("valuetooltip"),
        contents.findChild("valuetooltipencoded").data().toInt());
    setWatchDataValueDisabled(data, contents.findChild("valuedisabled"));
    setWatchDataEditValue(data, contents.findChild("editvalue"));
    if (qq->watchHandler()->isDisplayedIName(data.iname)) {
        GdbMi editvalue = contents.findChild("editvalue");
        if (editvalue.isValid()) {
            setWatchDataEditValue(data, editvalue);
            qq->watchHandler()->showEditValue(data);
        }
    }
    if (!qq->watchHandler()->isExpandedIName(data.iname))
        data.setChildrenUnneeded();
    GdbMi children = contents.findChild("children");
    if (children.isValid() || !qq->watchHandler()->isExpandedIName(data.iname))
        data.setChildrenUnneeded();
    data.setValueUnneeded();

    // try not to repeat data too often
    WatchData childtemplate;
    setWatchDataType(childtemplate, contents.findChild("childtype"));
    setWatchDataChildCount(childtemplate, contents.findChild("childnumchild"));
    //qDebug() << "DATA:" << data.toString();
    insertData(data);
    foreach (GdbMi item, children.children()) {
        WatchData data1 = childtemplate;
        data1.name = _(item.findChild("name").data());
        data1.iname = data.iname + _c('.') + data1.name;
        if (!data1.name.isEmpty() && data1.name.at(0).isDigit())
            data1.name = _c('[') + data1.name + _c(']');
        QByteArray key = item.findChild("key").data();
        if (!key.isEmpty()) {
            int encoding = item.findChild("keyencoded").data().toInt();
            QString skey = decodeData(key, encoding);
            if (skey.size() > 13) {
                skey = skey.left(12);
                skey += _("...");
            }
            //data1.name += " (" + skey + ")";
            data1.name = skey;
        }
        setWatchDataType(data1, item.findChild("type"));
        setWatchDataExpression(data1, item.findChild("exp"));
        setWatchDataChildCount(data1, item.findChild("numchild"));
        setWatchDataValue(data1, item.findChild("value"),
            item.findChild("valueencoded").data().toInt());
        setWatchDataAddress(data1, item.findChild("addr"));
        setWatchDataSAddress(data1, item.findChild("saddr"));
        setWatchDataValueToolTip(data1, item.findChild("valuetooltip"),
            contents.findChild("valuetooltipencoded").data().toInt());
        setWatchDataValueDisabled(data1, item.findChild("valuedisabled"));
        if (!qq->watchHandler()->isExpandedIName(data1.iname))
            data1.setChildrenUnneeded();
        //qDebug() << "HANDLE CUSTOM SUBCONTENTS:" << data1.toString();
        insertData(data1);
    }
}

void GdbEngine::handleDebuggingHelperValue3(const GdbResultRecord &record,
    const QVariant &cookie)
{
    if (record.resultClass == GdbResultDone) {
        WatchData data = cookie.value<WatchData>();
        QByteArray out = record.data.findChild("consolestreamoutput").data();
        while (out.endsWith(' ') || out.endsWith('\n'))
            out.chop(1);
        QList<QByteArray> list = out.split(' ');
        //qDebug() << "RECEIVED" << record.toString() << "FOR" << data0.toString()
        //    <<  " STREAM:" << out;
        if (list.isEmpty()) {
            //: Value for variable
            data.setValue(strNotInScope);
            data.setAllUnneeded();
            insertData(data);
        } else if (data.type == __("QString")
                || data.type.endsWith(__("::QString"))) {
            QList<QByteArray> list = out.split(' ');
            QString str;
            int l = out.isEmpty() ? 0 : list.size();
            for (int i = 0; i < l; ++i)
                 str.append(list.at(i).toInt());
            data.setValue(_c('"') + str + _c('"'));
            data.setChildCount(0);
            data.setAllUnneeded();
            insertData(data);
        } else if (data.type == __("QStringList")
                || data.type.endsWith(__("::QStringList"))) {
            if (out.isEmpty()) {
                data.setValue(tr("<0 items>"));
                data.setChildCount(0);
                data.setAllUnneeded();
                insertData(data);
            } else {
                int l = list.size();
                //: In string list
                data.setValue(tr("<%n items>", 0, l));
                data.setChildCount(list.size());
                data.setAllUnneeded();
                insertData(data);
                for (int i = 0; i < l; ++i) {
                    WatchData data1;
                    data1.name = _("[%1]").arg(i);
                    data1.type = data.type.left(data.type.size() - 4);
                    data1.iname = data.iname + _(".%1").arg(i);
                    data1.addr = _(list.at(i));
                    data1.exp = _("((") + gdbQuoteTypes(data1.type) + _("*)") + data1.addr + _c(')');
                    data1.setChildCount(0);
                    data1.setValueNeeded();
                    QString cmd = _("qdumpqstring (") + data1.exp + _c(')');
                    QVariant var;
                    var.setValue(data1);
                    postCommand(cmd, WatchUpdate, CB(handleDebuggingHelperValue3), var);
                }
            }
        } else {
            //: Value for variable
            data.setValue(strNotInScope);
            data.setAllUnneeded();
            insertData(data);
        }
    } else if (record.resultClass == GdbResultError) {
        WatchData data = cookie.value<WatchData>();
        data.setValue(strNotInScope);
        data.setAllUnneeded();
        insertData(data);
    }
}

void GdbEngine::updateLocals()
{
    // Asynchronous load of injected library, initialize in first stop
    if (m_dumperInjectionLoad && m_debuggingHelperState == DebuggingHelperLoadTried
        && m_dumperHelper.typeCount() == 0
        && q->inferiorPid() > 0)
        tryQueryDebuggingHelpers();

    m_pendingRequests = 0;

    PENDING_DEBUG("\nRESET PENDING");
    m_toolTipCache.clear();
    m_toolTipExpression.clear();
    qq->watchHandler()->reinitializeWatchers();

    QString level = QString::number(currentFrame());
    // '2' is 'list with type and value'
    QString cmd = _("-stack-list-arguments 2 ") + level + _c(' ') + level;
    postCommand(cmd, WatchUpdate, CB(handleStackListArguments));
    // '2' is 'list with type and value'
    postCommand(_("-stack-list-locals 2"), WatchUpdate,
        CB(handleStackListLocals)); // stage 2/2
}

void GdbEngine::handleStackListArguments(const GdbResultRecord &record, const QVariant &)
{
    // stage 1/2

    // Linux:
    // 12^done,stack-args=
    //   [frame={level="0",args=[
    //     {name="argc",type="int",value="1"},
    //     {name="argv",type="char **",value="(char **) 0x7..."}]}]
    // Mac:
    // 78^done,stack-args=
    //    {frame={level="0",args={
    //      varobj=
    //        {exp="this",value="0x38a2fab0",name="var21",numchild="3",
    //             type="CurrentDocumentFind *  const",typecode="PTR",
    //             dynamic_type="",in_scope="true",block_start_addr="0x3938e946",
    //             block_end_addr="0x3938eb2d"},
    //      varobj=
    //         {exp="before",value="@0xbfffb9f8: {d = 0x3a7f2a70}",
    //              name="var22",numchild="1",type="const QString  ...} }}}
    //
    // In both cases, iterating over the children of stack-args/frame/args
    // is ok.
    m_currentFunctionArgs.clear();
    if (record.resultClass == GdbResultDone) {
        const GdbMi list = record.data.findChild("stack-args");
        const GdbMi frame = list.findChild("frame");
        const GdbMi args = frame.findChild("args");
        m_currentFunctionArgs = args.children();
    } else if (record.resultClass == GdbResultError) {
        qDebug() << "FIXME: GdbEngine::handleStackListArguments: should not happen";
    }
}

void GdbEngine::handleStackListLocals(const GdbResultRecord &record, const QVariant &)
{
    // stage 2/2

    // There could be shadowed variables
    QList<GdbMi> locals = record.data.findChild("locals").children();
    locals += m_currentFunctionArgs;

    setLocals(locals);
}

void GdbEngine::setLocals(const QList<GdbMi> &locals)
{
    //qDebug() << m_varToType;
    QMap<QByteArray, int> seen;

    foreach (const GdbMi &item, locals) {
        // Local variables of inlined code are reported as
        // 26^done,locals={varobj={exp="this",value="",name="var4",exp="this",
        // numchild="1",type="const QtSharedPointer::Basic<CPlusPlus::..."
        // We do not want these at all. Current hypotheses is that those
        // "spurious" locals have _two_ "exp" field. Try to filter them:
        #ifdef Q_OS_MAC
        int numExps = 0;
        foreach (const GdbMi &child, item.children())
            numExps += int(child.name() == "exp");
        if (numExps > 1)
            continue;
        QByteArray name = item.findChild("exp").data();
        #else
        QByteArray name = item.findChild("name").data();
        #endif
        int n = seen.value(name);
        if (n) {
            seen[name] = n + 1;
            WatchData data;
            QString nam = _(name);
            data.iname = _("local.") + nam + QString::number(n + 1);
            //: Variable %1 <FIXME: does something - bug Andre about it>
            data.name = tr("%1 <shadowed %2>").arg(nam, n);
            //data.setValue("<shadowed>");
            setWatchDataValue(data, item.findChild("value"));
            //: Type of variable <FIXME: what? bug Andre about it>
            data.setType(tr("<shadowed>"));
            data.setChildCount(0);
            insertData(data);
        } else {
            seen[name] = 1;
            WatchData data;
            QString nam = _(name);
            data.iname = _("local.") + nam;
            data.name = nam;
            data.exp = nam;
            data.framekey = m_currentFrame + data.name;
            setWatchDataType(data, item.findChild("type"));
            // set value only directly if it is simple enough, otherwise
            // pass through the insertData() machinery
            if (isIntOrFloatType(data.type) || isPointerType(data.type))
                setWatchDataValue(data, item.findChild("value"));
            if (!qq->watchHandler()->isExpandedIName(data.iname))
                data.setChildrenUnneeded();
            if (isPointerType(data.type) || data.name == __("this"))
                data.setChildCount(1);
            if (0 && m_varToType.contains(data.framekey)) {
                qDebug() << "RE-USING" << m_varToType.value(data.framekey);
                data.setType(m_varToType.value(data.framekey));
            }
            insertData(data);
        }
    }
}

void GdbEngine::insertData(const WatchData &data0)
{
    //qDebug() << "INSERT DATA" << data0.toString();
    WatchData data = data0;
    if (data.value.startsWith(__("mi_cmd_var_create:"))) {
        qDebug() << "BOGUS VALUE:" << data.toString();
        return;
    }
    qq->watchHandler()->insertData(data);
}

void GdbEngine::handleTypeContents(const QString &output)
{
    // output.startsWith("type = ") == true
    // "type = int"
    // "type = class QString {"
    // "type = class QStringList : public QList<QString> {"
    QString tip;
    QString className;
    if (output.startsWith(__("type = class"))) {
        int posBrace = output.indexOf(_c('{'));
        QString head = output.mid(13, posBrace - 13 - 1);
        int posColon = head.indexOf(__(": public"));
        if (posColon == -1)
            posColon = head.indexOf(__(": protected"));
        if (posColon == -1)
            posColon = head.indexOf(__(": private"));
        if (posColon == -1) {
            className = head;
            tip = _("class ") + className + _(" { ... }");
        } else {
            className = head.left(posColon - 1);
            tip = _("class ") + head + _(" { ... }");
        }
        //qDebug() << "posColon:" << posColon;
        //qDebug() << "posBrace:" << posBrace;
        //qDebug() << "head:" << head;
    } else {
        className = output.mid(7);
        tip = className;
    }
    //qDebug() << "output:" << output.left(100) + "...";
    //qDebug() << "className:" << className;
    //qDebug() << "tip:" << tip;
    //m_toolTip.type = className;
    m_toolTip.type.clear();
    m_toolTip.value = tip;
}

void GdbEngine::handleVarListChildrenHelper(const GdbMi &item,
    const WatchData &parent)
{
    //qDebug() <<  "VAR_LIST_CHILDREN: PARENT 2" << parent.toString();
    //qDebug() <<  "VAR_LIST_CHILDREN: APPENDEE" << data.toString();
    QByteArray exp = item.findChild("exp").data();
    QByteArray name = item.findChild("name").data();
    if (isAccessSpecifier(_(exp))) {
        // suppress 'private'/'protected'/'public' level
        WatchData data;
        data.variable = _(name);
        data.iname = parent.iname;
        //data.iname = data.variable;
        data.exp = parent.exp;
        data.setTypeUnneeded();
        data.setValueUnneeded();
        data.setChildCountUnneeded();
        data.setChildrenUnneeded();
        //qDebug() << "DATA" << data.toString();
        QString cmd = _("-var-list-children --all-values \"") + data.variable + _c('"');
        //iname += '.' + exp;
        postCommand(cmd, WatchUpdate, CB(handleVarListChildren), QVariant::fromValue(data));
    } else if (item.findChild("numchild").data() == "0") {
        // happens for structs without data, e.g. interfaces.
        WatchData data;
        data.name = _(exp);
        data.iname = parent.iname + _c('.') + data.name;
        data.variable = _(name);
        setWatchDataType(data, item.findChild("type"));
        setWatchDataValue(data, item.findChild("value"));
        setWatchDataAddress(data, item.findChild("addr"));
        setWatchDataSAddress(data, item.findChild("saddr"));
        data.setChildCount(0);
        insertData(data);
    } else if (parent.iname.endsWith(_c('.'))) {
        // Happens with anonymous unions
        WatchData data;
        data.iname = _(name);
        QString cmd = _("-var-list-children --all-values \"") + data.variable + _c('"');
        postCommand(cmd, WatchUpdate, CB(handleVarListChildren), QVariant::fromValue(data));
    } else if (exp == "staticMetaObject") {
        //    && item.findChild("type").data() == "const QMetaObject")
        // FIXME: Namespaces?
        // { do nothing }    FIXME: make configurable?
        // special "clever" hack to avoid clutter in the GUI.
        // I am not sure this is a good idea...
    } else {
        WatchData data;
        data.iname = parent.iname + _c('.') + __(exp);
        data.variable = _(name);
        setWatchDataType(data, item.findChild("type"));
        setWatchDataValue(data, item.findChild("value"));
        setWatchDataAddress(data, item.findChild("addr"));
        setWatchDataSAddress(data, item.findChild("saddr"));
        setWatchDataChildCount(data, item.findChild("numchild"));
        if (!qq->watchHandler()->isExpandedIName(data.iname))
            data.setChildrenUnneeded();

        data.name = _(exp);
        if (data.type == data.name) {
            if (isPointerType(parent.type)) {
                data.exp = _("*(") + parent.exp + _c(')');
                data.name = _("*") + parent.name;
            } else {
                // A type we derive from? gdb crashes when creating variables here
                data.exp = parent.exp;
            }
        } else if (exp.startsWith("*")) {
            // A pointer
            data.exp = _("*(") + parent.exp + _c(')');
        } else if (startsWithDigit(data.name)) {
            // An array. No variables needed?
            data.name = _c('[') + data.name + _c(']');
            data.exp = parent.exp + _('[' + exp + ']');
        } else if (0 && parent.name.endsWith(_c('.'))) {
            // Happens with anonymous unions
            data.exp = parent.exp + data.name;
            //data.name = "<anonymous union>";
        } else if (exp.isEmpty()) {
            // Happens with anonymous unions
            data.exp = parent.exp;
            data.name = tr("<n/a>");
            data.iname = parent.iname + _(".@");
            data.type = tr("<anonymous union>");
        } else {
            // A structure. Hope there's nothing else...
            data.exp = parent.exp + _c('.') + data.name;
        }

        if (hasDebuggingHelperForType(data.type)) {
            // we do not trust gdb if we have a custom dumper
            data.setValueNeeded();
            data.setChildCountNeeded();
        }

        //qDebug() <<  "VAR_LIST_CHILDREN: PARENT 3" << parent.toString();
        //qDebug() <<  "VAR_LIST_CHILDREN: APPENDEE" << data.toString();
        insertData(data);
    }
}

void GdbEngine::handleVarListChildren(const GdbResultRecord &record,
    const QVariant &cookie)
{
    //WatchResultCounter dummy(this, WatchVarListChildren);
    WatchData data = cookie.value<WatchData>();
    if (!data.isValid())
        return;
    if (record.resultClass == GdbResultDone) {
        //qDebug() <<  "VAR_LIST_CHILDREN: PARENT" << data.toString();
        GdbMi children = record.data.findChild("children");

        foreach (const GdbMi &child, children.children())
            handleVarListChildrenHelper(child, data);

        if (children.children().isEmpty()) {
            // happens e.g. if no debug information is present or
            // if the class really has no children
            WatchData data1;
            data1.iname = data.iname + _(".child");
            //: About variable's value
            data1.value = tr("<no information>");
            data1.childCount = 0;
            data1.setAllUnneeded();
            insertData(data1);
            data.setAllUnneeded();
            insertData(data);
        } else if (!isAccessSpecifier(data.variable.split(_c('.')).last())) {
            data.setChildrenUnneeded();
            insertData(data);
        } else {
            // this skips the spurious "public", "private" etc levels
            // gdb produces
        }
    } else if (record.resultClass == GdbResultError) {
        data.setError(QString::fromLocal8Bit(record.data.findChild("msg").data()));
    } else {
        data.setError(tr("Unknown error: ") + QString::fromLocal8Bit(record.toString()));
    }
}

void GdbEngine::handleToolTip(const GdbResultRecord &record,
        const QVariant &cookie)
{
    const QByteArray &what = cookie.toByteArray();
    //qDebug() << "HANDLE TOOLTIP:" << what << m_toolTip.toString();
    //    << "record: " << record.toString();
    if (record.resultClass == GdbResultError) {
        if (what == "create") {
            postCommand(_("ptype ") + m_toolTip.exp,
                Discardable, CB(handleToolTip), QByteArray("ptype"));
            return;
        }
        if (what == "evaluate") {
            QByteArray msg = record.data.findChild("msg").data();
            if (msg.startsWith("Cannot look up value of a typedef")) {
                m_toolTip.value = tr("%1 is a typedef.").arg(m_toolTip.exp);
                //return;
            }
        }
    } else if (record.resultClass == GdbResultDone) {
        if (what == "create") {
            setWatchDataType(m_toolTip, record.data.findChild("type"));
            setWatchDataChildCount(m_toolTip, record.data.findChild("numchild"));
            if (hasDebuggingHelperForType(m_toolTip.type))
                runDebuggingHelper(m_toolTip, false);
            else
                q->showStatusMessage(tr("Retrieving data for tooltip..."), 10000);
                postCommand(_("-data-evaluate-expression ") + m_toolTip.exp,
                    Discardable, CB(handleToolTip), QByteArray("evaluate"));
            return;
        }
        if (what == "evaluate") {
            m_toolTip.value = m_toolTip.type + _c(' ') + m_toolTip.exp
                   + _(" = " + record.data.findChild("value").data());
            //return;
        }
        if (what == "ptype") {
            GdbMi mi = record.data.findChild("consolestreamoutput");
            m_toolTip.value = extractTypeFromPTypeOutput(_(mi.data()));
            //return;
        }
    }

    m_toolTip.iname = tooltipIName;
    m_toolTip.setChildrenUnneeded();
    m_toolTip.setChildCountUnneeded();
    insertData(m_toolTip);
    qDebug() << "DATA INSERTED";
    QTimer::singleShot(0, this, SLOT(updateWatchModel2()));
    qDebug() << "HANDLE TOOLTIP END";
}

#if 0
void GdbEngine::handleChangedItem(QStandardItem *item)
{
    // HACK: Just store the item for the slot
    //  handleChangedItem(QWidget *widget) below.
    QModelIndex index = item->index().sibling(item->index().row(), 0);
    //WatchData data = m_currentSet.takeData(iname);
    //m_editedData = inameFromItem(m_model.itemFromIndex(index)).exp;
    //qDebug() << "HANDLE CHANGED EXPRESSION:" << m_editedData;
}
#endif

void GdbEngine::assignValueInDebugger(const QString &expression, const QString &value)
{
    postCommand(_("-var-delete assign"));
    postCommand(_("-var-create assign * ") + expression);
    postCommand(_("-var-assign assign ") + value, Discardable, CB(handleVarAssign));
}

void GdbEngine::tryLoadDebuggingHelpers()
{
    if (m_debuggingHelperState != DebuggingHelperUninitialized)
        return;
    if (!startModeAllowsDumpers()) {
        // load gdb macro based dumpers at least 
        QFile file(_(":/gdb/gdbmacros.txt"));
        file.open(QIODevice::ReadOnly);
        QByteArray contents = file.readAll(); 
        //qDebug() << "CONTENTS: " << contents;
        postCommand(_(contents));
        return;
    }
    if (m_dumperInjectionLoad && q->inferiorPid() <= 0) // Need PID to inject
        return;

    PENDING_DEBUG("TRY LOAD CUSTOM DUMPERS");
    m_debuggingHelperState = DebuggingHelperUnavailable;
    if (!qq->qtDumperLibraryEnabled())
        return;
    const QString lib = qq->qtDumperLibraryName();
    //qDebug() << "DUMPERLIB:" << lib;
    // @TODO: same in CDB engine...
    const QFileInfo fi(lib);
    if (!fi.exists()) {
        const QString msg = tr("The dumper library '%1' does not exist.").arg(lib);
        debugMessage(msg);
        qq->showQtDumperLibraryWarning(msg);
        return;
    }

    m_debuggingHelperState = DebuggingHelperLoadTried;
#if defined(Q_OS_WIN)
    if (m_dumperInjectionLoad) {
        /// Launch asynchronous remote thread to load.
        SharedLibraryInjector injector(q->inferiorPid());
        QString errorMessage;
        if (injector.remoteInject(lib, false, &errorMessage)) {
            debugMessage(tr("Dumper injection loading triggered (%1)...").arg(lib));
        } else {
            debugMessage(tr("Dumper loading (%1) failed: %2").arg(lib, errorMessage));
            debugMessage(errorMessage);
            qq->showQtDumperLibraryWarning(errorMessage);
            m_debuggingHelperState = DebuggingHelperUnavailable;
            return;
        }
    } else {
        debugMessage(tr("Loading dumpers via debugger call (%1)...").arg(lib));
        postCommand(_("sharedlibrary .*")); // for LoadLibraryA
        //postCommand(_("handle SIGSEGV pass stop print"));
        //postCommand(_("set unwindonsignal off"));
        postCommand(_("call LoadLibraryA(\"") + GdbMi::escapeCString(lib) + _("\")"),
                    CB(handleDebuggingHelperSetup));
        postCommand(_("sharedlibrary ") + dotEscape(lib));
    }
#elif defined(Q_OS_MAC)
    //postCommand(_("sharedlibrary libc")); // for malloc
    //postCommand(_("sharedlibrary libdl")); // for dlopen
    postCommand(_("call (void)dlopen(\"") + GdbMi::escapeCString(lib) + _("\", " STRINGIFY(RTLD_NOW) ")"),
        CB(handleDebuggingHelperSetup));
    //postCommand(_("sharedlibrary ") + dotEscape(lib));
    m_debuggingHelperState = DebuggingHelperLoadTried;
#else
    //postCommand(_("p dlopen"));
    QString flag = QString::number(RTLD_NOW);
    postCommand(_("sharedlibrary libc")); // for malloc
    postCommand(_("sharedlibrary libdl")); // for dlopen
    postCommand(_("call (void*)dlopen(\"") + GdbMi::escapeCString(lib) + _("\", " STRINGIFY(RTLD_NOW) ")"),
        CB(handleDebuggingHelperSetup));
    // some older systems like CentOS 4.6 prefer this:
    postCommand(_("call (void*)__dlopen(\"") + GdbMi::escapeCString(lib) + _("\", " STRINGIFY(RTLD_NOW) ")"),
        CB(handleDebuggingHelperSetup));
    postCommand(_("sharedlibrary ") + dotEscape(lib));
#endif
    if (!m_dumperInjectionLoad)
        tryQueryDebuggingHelpers();
}

void GdbEngine::tryQueryDebuggingHelpers()
{
    // retrieve list of dumpable classes
    postCommand(_("call (void*)qDumpObjectData440(1,%1+1,0,0,0,0,0,0)"), EmbedToken);
    postCommand(_("p (char*)&qDumpOutBuffer"), CB(handleQueryDebuggingHelper));
}

void GdbEngine::recheckDebuggingHelperAvailability()
{
    if (startModeAllowsDumpers()) {
        // retreive list of dumpable classes
        postCommand(_("call (void*)qDumpObjectData440(1,%1+1,0,0,0,0,0,0)"), EmbedToken);
        postCommand(_("p (char*)&qDumpOutBuffer"), CB(handleQueryDebuggingHelper));
    }
}

bool GdbEngine::startModeAllowsDumpers() const
{
    return q->startMode() == StartInternal
        || q->startMode() == StartExternal
        || q->startMode() == AttachExternal;
}

IDebuggerEngine *createGdbEngine(DebuggerManager *parent, QList<Core::IOptionsPage*> *opts)
{
    opts->push_back(new GdbOptionsPage);
    return new GdbEngine(parent);
}
