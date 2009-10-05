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

#include "cdbdebugengine.h"
#include "cdbdebugengine_p.h"
#include "cdbstacktracecontext.h"
#include "cdbstackframecontext.h"
#include "cdbsymbolgroupcontext.h"
#include "cdbbreakpoint.h"
#include "cdbmodules.h"
#include "cdbassembler.h"
#include "cdboptionspage.h"
#include "cdboptions.h"

#include "debuggeractions.h"
#include "debuggermanager.h"
#include "breakhandler.h"
#include "stackhandler.h"
#include "watchhandler.h"
#include "registerhandler.h"
#include "moduleshandler.h"
#include "disassemblerhandler.h"
#include "watchutils.h"

#include <coreplugin/icore.h>
#include <utils/qtcassert.h>
#include <utils/winutils.h>
#include <utils/consoleprocess.h>
#include <texteditor/itexteditor.h>

#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QTimerEvent>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QLibrary>
#include <QtCore/QCoreApplication>
#include <QtGui/QMessageBox>
#include <QtGui/QMainWindow>
#include <QtGui/QApplication>
#include <QtGui/QToolTip>

#define DBGHELP_TRANSLATE_TCHAR
#include <inc/Dbghelp.h>

static const char *dbgHelpDllC = "dbghelp";
static const char *dbgEngineDllC = "dbgeng";
static const char *debugCreateFuncC = "DebugCreate";

static const char *localSymbolRootC = "local";

namespace Debugger {
namespace Internal {

typedef QList<WatchData> WatchList;

// ----- Message helpers

QString msgDebugEngineComResult(HRESULT hr)
{
    switch (hr) {
        case S_OK:
        return QLatin1String("S_OK");
        case S_FALSE:
        return QLatin1String("S_FALSE");
        case E_FAIL:
        break;
        case E_INVALIDARG:
        return QLatin1String("E_INVALIDARG");
        case E_NOINTERFACE:
        return QLatin1String("E_NOINTERFACE");
        case E_OUTOFMEMORY:
        return QLatin1String("E_OUTOFMEMORY");
        case E_UNEXPECTED:
        return QLatin1String("E_UNEXPECTED");
        case E_NOTIMPL:
        return QLatin1String("E_NOTIMPL");
    }
    if (hr == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED))
        return QLatin1String("ERROR_ACCESS_DENIED");;
    if (hr == HRESULT_FROM_NT(STATUS_CONTROL_C_EXIT))
        return QLatin1String("STATUS_CONTROL_C_EXIT");
    return QLatin1String("E_FAIL ") + Core::Utils::winErrorMessage(HRESULT_CODE(hr));
}

static QString msgStackIndexOutOfRange(int idx, int size)
{
    return QString::fromLatin1("Frame index %1 out of range (%2).").arg(idx).arg(size);
}

QString msgComFailed(const char *func, HRESULT hr)
{
    return QString::fromLatin1("%1 failed: %2").arg(QLatin1String(func), msgDebugEngineComResult(hr));
}

static const char *msgNoStackTraceC = "Internal error: no stack trace present.";

static inline QString msgLibLoadFailed(const QString &lib, const QString &why)
{
    return CdbDebugEngine::tr("Unable to load the debugger engine library '%1': %2").
            arg(lib, why);
}

// Format function failure message. Pass in Q_FUNC_INFO
static QString msgFunctionFailed(const char *func, const QString &why)
{
    // Strip a "cdecl_ int namespace1::class::foo(int bar)" as
    // returned by Q_FUNC_INFO down to "foo"
    QString function = QLatin1String(func);
    const int firstParentPos = function.indexOf(QLatin1Char('('));
    if (firstParentPos != -1)
        function.truncate(firstParentPos);
    const int classSepPos = function.lastIndexOf(QLatin1String("::"));
    if (classSepPos != -1)
        function.remove(0, classSepPos + 2);
   //: Function call failed
   return CdbDebugEngine::tr("The function \"%1()\" failed: %2").arg(function, why);
}

// ----- Engine helpers

static inline ULONG getInterruptTimeOutSecs(CIDebugControl *ctl)
{
    ULONG rc = 0;
    ctl->GetInterruptTimeout(&rc);
    return rc;
}

bool getExecutionStatus(CIDebugControl *ctl,
                        ULONG *executionStatus,
                        QString *errorMessage /* = 0 */)
{
    const HRESULT hr = ctl->GetExecutionStatus(executionStatus);
    if (FAILED(hr)) {
        if (errorMessage)
            *errorMessage = msgComFailed("GetExecutionStatus", hr);
        return false;
    }
    return true;
}

const char *executionStatusString(ULONG executionStatus)
{
    switch (executionStatus) {
    case DEBUG_STATUS_NO_CHANGE:
        return "DEBUG_STATUS_NO_CHANGE";
    case DEBUG_STATUS_GO:
        return "DEBUG_STATUS_GO";
    case DEBUG_STATUS_GO_HANDLED:
        return "DEBUG_STATUS_GO_HANDLED";
    case DEBUG_STATUS_GO_NOT_HANDLED:
        return "DEBUG_STATUS_GO_NOT_HANDLED";
    case DEBUG_STATUS_STEP_OVER:
        return "DEBUG_STATUS_STEP_OVER";
    case DEBUG_STATUS_STEP_INTO:
        return "DEBUG_STATUS_STEP_INTO";
    case DEBUG_STATUS_BREAK:
        return "DEBUG_STATUS_BREAK";
    case DEBUG_STATUS_NO_DEBUGGEE:
        return "DEBUG_STATUS_NO_DEBUGGEE";
    case DEBUG_STATUS_STEP_BRANCH:
        return "DEBUG_STATUS_STEP_BRANCH";
    case DEBUG_STATUS_IGNORE_EVENT:
        return "DEBUG_STATUS_IGNORE_EVENT";
    case DEBUG_STATUS_RESTART_REQUESTED:
        return "DEBUG_STATUS_RESTART_REQUESTED";
    case DEBUG_STATUS_REVERSE_GO:
        return "DEBUG_STATUS_REVERSE_GO";
          case DEBUG_STATUS_REVERSE_STEP_BRANCH:
        return "DEBUG_STATUS_REVERSE_STEP_BRANCH";
    case DEBUG_STATUS_REVERSE_STEP_OVER:
        return "DEBUG_STATUS_REVERSE_STEP_OVER";
    case DEBUG_STATUS_REVERSE_STEP_INTO:
        return "DEBUG_STATUS_REVERSE_STEP_INTO";
        default:
        break;
    }
    return "<Unknown execution status>";
}

// Debug convenience
const char *executionStatusString(CIDebugControl *ctl)
{
    ULONG executionStatus;
    if (getExecutionStatus(ctl, &executionStatus))
        return executionStatusString(executionStatus);
    return "<failed>";
}

// --------- DebuggerEngineLibrary
DebuggerEngineLibrary::DebuggerEngineLibrary() :
    m_debugCreate(0)
{
}

// Build a lib name as "Path\x.dll"
static inline QString libPath(const QString &libName, const QString &path = QString())
{
    QString rc = path;
    if (!rc.isEmpty())
        rc += QDir::separator();
    rc += libName;
    rc += QLatin1String(".dll");
    return rc;
}

bool DebuggerEngineLibrary::init(const QString &path, QString *errorMessage)
{
    // Load the dependent help lib first
    const QString helpLibPath = libPath(QLatin1String(dbgHelpDllC), path);
    QLibrary helpLib(helpLibPath, 0);
    if (!helpLib.isLoaded() && !helpLib.load()) {
        *errorMessage = msgLibLoadFailed(helpLibPath, helpLib.errorString());
        return false;
    }
    // Load dbgeng lib
    const QString engineLibPath = libPath(QLatin1String(dbgEngineDllC), path);
    QLibrary lib(engineLibPath, 0);
    if (!lib.isLoaded() && !lib.load()) {
        *errorMessage = msgLibLoadFailed(engineLibPath, lib.errorString());
        return false;
    }
    // Locate symbols
    void *createFunc = lib.resolve(debugCreateFuncC);
    if (!createFunc) {
        *errorMessage = CdbDebugEngine::tr("Unable to resolve '%1' in the debugger engine library '%2'").
                        arg(QLatin1String(debugCreateFuncC), QLatin1String(dbgEngineDllC));
        return false;
    }
    m_debugCreate = static_cast<DebugCreateFunction>(createFunc);
    return true;
}

// ----- SyntaxSetter
SyntaxSetter::SyntaxSetter(CIDebugControl *ctl, ULONG desiredSyntax) :
    m_desiredSyntax(desiredSyntax),
    m_ctl(ctl)
{
    m_ctl->GetExpressionSyntax(&m_oldSyntax);
    if (m_oldSyntax != m_desiredSyntax)
        m_ctl->SetExpressionSyntax(m_desiredSyntax);
}

SyntaxSetter::~SyntaxSetter()
{
    if (m_oldSyntax != m_desiredSyntax)
        m_ctl->SetExpressionSyntax(m_oldSyntax);
}

// CdbComInterfaces
CdbComInterfaces::CdbComInterfaces() :
    debugClient(0),
    debugControl(0),
    debugSystemObjects(0),
    debugSymbols(0),
    debugRegisters(0),
    debugDataSpaces(0)
{
}

// --- CdbDebugEnginePrivate

CdbDebugEnginePrivate::CdbDebugEnginePrivate(DebuggerManager *parent,
                                             const QSharedPointer<CdbOptions> &options,
                                             CdbDebugEngine* engine) :
    m_options(options),
    m_hDebuggeeProcess(0),
    m_hDebuggeeThread(0),
    m_breakEventMode(BreakEventHandle),
    m_dumper(new CdbDumperHelper(parent, &m_cif)),
    m_watchTimer(-1),
    m_debugEventCallBack(engine),
    m_engine(engine),
    m_debuggerManager(parent),
    m_debuggerManagerAccess(parent->engineInterface()),
    m_currentStackTrace(0),
    m_firstActivatedFrame(true),
    m_mode(AttachCore)
{
}

bool CdbDebugEnginePrivate::init(QString *errorMessage)
{
    // Load the DLL
    DebuggerEngineLibrary lib;
    if (!lib.init(m_options->path, errorMessage))
        return false;

    // Initialize the COM interfaces
    HRESULT hr;
    hr = lib.debugCreate( __uuidof(IDebugClient5), reinterpret_cast<void**>(&m_cif.debugClient));
    if (FAILED(hr)) {
        *errorMessage = QString::fromLatin1("Creation of IDebugClient5 failed: %1").arg(msgDebugEngineComResult(hr));
        return false;
    }

    m_cif.debugClient->SetOutputCallbacksWide(&m_debugOutputCallBack);
    m_cif.debugClient->SetEventCallbacksWide(&m_debugEventCallBack);

    hr = lib.debugCreate( __uuidof(IDebugControl4), reinterpret_cast<void**>(&m_cif.debugControl));
    if (FAILED(hr)) {
        *errorMessage = QString::fromLatin1("Creation of IDebugControl4 failed: %1").arg(msgDebugEngineComResult(hr));
        return false;
    }

    m_cif.debugControl->SetCodeLevel(DEBUG_LEVEL_SOURCE);

    hr = lib.debugCreate( __uuidof(IDebugSystemObjects4), reinterpret_cast<void**>(&m_cif.debugSystemObjects));
    if (FAILED(hr)) {
        *errorMessage = QString::fromLatin1("Creation of IDebugSystemObjects4 failed: %1").arg(msgDebugEngineComResult(hr));
        return false;
    }

    hr = lib.debugCreate( __uuidof(IDebugSymbols3), reinterpret_cast<void**>(&m_cif.debugSymbols));
    if (FAILED(hr)) {
        *errorMessage = QString::fromLatin1("Creation of IDebugSymbols3 failed: %1").arg(msgDebugEngineComResult(hr));
        return false;
    }

    hr = lib.debugCreate( __uuidof(IDebugRegisters2), reinterpret_cast<void**>(&m_cif.debugRegisters));
    if (FAILED(hr)) {
        *errorMessage = QString::fromLatin1("Creation of IDebugRegisters2 failed: %1").arg(msgDebugEngineComResult(hr));
        return false;
    }

    hr = lib.debugCreate( __uuidof(IDebugDataSpaces4), reinterpret_cast<void**>(&m_cif.debugDataSpaces));
    if (FAILED(hr)) {
        *errorMessage = QString::fromLatin1("Creation of IDebugDataSpaces4 failed: %1").arg(msgDebugEngineComResult(hr));
        return false;
    }

    if (debugCDB)
        qDebug() << QString::fromLatin1("CDB Initialization succeeded, interrupt time out %1s.").arg(getInterruptTimeOutSecs(m_cif.debugControl));
    return true;
}

IDebuggerEngine *CdbDebugEngine::create(DebuggerManager *parent,
                                        const QSharedPointer<CdbOptions> &options,
                                        QString *errorMessage)
{
    CdbDebugEngine *rc = new CdbDebugEngine(parent, options);
    if (rc->m_d->init(errorMessage)) {
        rc->syncDebuggerPaths();
        return rc;
    }
    delete rc;
    return 0;
}

CdbDebugEnginePrivate::~CdbDebugEnginePrivate()
{
    cleanStackTrace();
    if (m_cif.debugClient)
        m_cif.debugClient->Release();
    if (m_cif.debugControl)
        m_cif.debugControl->Release();
    if (m_cif.debugSystemObjects)
        m_cif.debugSystemObjects->Release();
    if (m_cif.debugSymbols)
        m_cif.debugSymbols->Release();
    if (m_cif.debugRegisters)
        m_cif.debugRegisters->Release();
    if (m_cif.debugDataSpaces)
        m_cif.debugDataSpaces->Release();
}

void CdbDebugEnginePrivate::clearForRun()
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO;

    m_breakEventMode = BreakEventHandle;
    m_firstActivatedFrame = false;
    cleanStackTrace();
    m_editorToolTipCache.clear();
}

void CdbDebugEnginePrivate::cleanStackTrace()
{
    if (m_currentStackTrace) {
        delete m_currentStackTrace;
        m_currentStackTrace = 0;
    }
}

CdbDebugEngine::CdbDebugEngine(DebuggerManager *parent, const QSharedPointer<CdbOptions> &options) :
    IDebuggerEngine(parent),
    m_d(new CdbDebugEnginePrivate(parent, options, this))
{
    m_d->m_consoleStubProc.setMode(Core::Utils::ConsoleProcess::Suspend);
    connect(&m_d->m_consoleStubProc, SIGNAL(processError(QString)), this, SLOT(slotConsoleStubError(QString)));
    connect(&m_d->m_consoleStubProc, SIGNAL(processStarted()), this, SLOT(slotConsoleStubStarted()));
    connect(&m_d->m_consoleStubProc, SIGNAL(wrapperStopped()), this, SLOT(slotConsoleStubTerminated()));
    connect(&m_d->m_debugOutputCallBack, SIGNAL(debuggerOutput(QString,QString)),
            m_d->m_debuggerManager, SLOT(showDebuggerOutput(QString,QString)));
    connect(&m_d->m_debugOutputCallBack, SIGNAL(debuggerInputPrompt(QString,QString)),
            m_d->m_debuggerManager, SLOT(showDebuggerInput(QString,QString)));
    connect(&m_d->m_debugOutputCallBack, SIGNAL(debuggeeOutput(QString)),
            m_d->m_debuggerManager, SLOT(showApplicationOutput(QString)));
    connect(&m_d->m_debugOutputCallBack, SIGNAL(debuggeeInputPrompt(QString)),
            m_d->m_debuggerManager, SLOT(showApplicationOutput(QString)));
}

CdbDebugEngine::~CdbDebugEngine()
{
    delete m_d;
}

void CdbDebugEngine::startWatchTimer()
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO;

    if (m_d->m_watchTimer == -1)
        m_d->m_watchTimer = startTimer(0);
}

void CdbDebugEngine::killWatchTimer()
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO;

    if (m_d->m_watchTimer != -1) {
        killTimer(m_d->m_watchTimer);
        m_d->m_watchTimer = -1;
    }
}

void CdbDebugEngine::shutdown()
{
    exitDebugger();
}

QString CdbDebugEngine::editorToolTip(const QString &exp, const QString &function)
{
    // Figure the editor tooltip. Ask the frame context of the
    // function if it is a local variable it knows. If that is not
    // the case, try to evaluate via debugger
    QString errorMessage;
    QString rc;
    // Find the frame of the function if there is any
    CdbStackFrameContext *frame = 0;
    if (m_d->m_currentStackTrace &&  !function.isEmpty()) {
        const int frameIndex = m_d->m_currentStackTrace->indexOf(function);
        if (frameIndex != -1)
            frame = m_d->m_currentStackTrace->frameContextAt(frameIndex, &errorMessage);
    }
    if (frame && frame->editorToolTip(QLatin1String("local.") + exp, &rc, &errorMessage))
        return rc;
    // No function/symbol context found, try to evaluate in current context.
    // Do not append type as this will mostly be 'long long' for integers, etc.
    QString type;
    if (!evaluateExpression(exp, &rc, &type, &errorMessage))
        return QString();
    return rc;
}

void CdbDebugEngine::setToolTipExpression(const QPoint &mousePos, TextEditor::ITextEditor *editor, int cursorPos)
{
    typedef CdbDebugEnginePrivate::EditorToolTipCache EditorToolTipCache;
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << '\n' << cursorPos;
    // Need a stopped debuggee and a cpp file
    if (!m_d->m_hDebuggeeProcess || m_d->isDebuggeeRunning())
        return;
    if (!isCppEditor(editor))
        return;
    // Determine expression and function
    QString toolTip;
    do {
        int line;
        int column;
        QString function;
        const QString exp = cppExpressionAt(editor, cursorPos, &line, &column, &function);
        if (function.isEmpty() || exp.isEmpty())
            break;
        // Check cache (key containing function) or try to figure out expression
        QString cacheKey = function;
        cacheKey += QLatin1Char('@');
        cacheKey += exp;
        const EditorToolTipCache::const_iterator cit = m_d->m_editorToolTipCache.constFind(cacheKey);
        if (cit != m_d->m_editorToolTipCache.constEnd()) {
            toolTip = cit.value();
        } else {
            toolTip = editorToolTip(exp, function);
            if (!toolTip.isEmpty())
                m_d->m_editorToolTipCache.insert(cacheKey, toolTip);
        }
    } while (false);
    // Display
    QToolTip::hideText();
    if (!toolTip.isEmpty())
        QToolTip::showText(mousePos, toolTip);
}

void CdbDebugEnginePrivate::clearDisplay()
{
    m_debuggerManagerAccess->threadsHandler()->removeAll();
    m_debuggerManagerAccess->modulesHandler()->removeAll();
    m_debuggerManagerAccess->registerHandler()->removeAll();
}

bool CdbDebugEngine::startDebugger(const QSharedPointer<DebuggerStartParameters> &sp)
{
    if (m_d->m_hDebuggeeProcess) {
        warning(QLatin1String("Internal error: Attempt to start debugger while another process is being debugged."));
        return false;
    }
    m_d->clearDisplay();

    const DebuggerStartMode mode = m_d->m_debuggerManager->startMode();
    // Figure out dumper. @TODO: same in gdb...
    const QString dumperLibName = QDir::toNativeSeparators(m_d->m_debuggerManagerAccess->qtDumperLibraryName());
    bool dumperEnabled = mode != AttachCore
                         && mode != AttachCrashedExternal
                         && !dumperLibName.isEmpty()
                         && m_d->m_debuggerManagerAccess->qtDumperLibraryEnabled();
    if (dumperEnabled) {
        const QFileInfo fi(dumperLibName);
        if (!fi.isFile()) {
            const QString msg = tr("The dumper library '%1' does not exist.").arg(dumperLibName);
            m_d->m_debuggerManagerAccess->showQtDumperLibraryWarning(msg);
            dumperEnabled = false;
        }
    }
    m_d->m_dumper->reset(dumperLibName, dumperEnabled);
    m_d->m_debuggerManager->showStatusMessage("Starting Debugger", -1);
    QString errorMessage;
    bool rc = false;
    bool needWatchTimer = false;
    m_d->clearForRun();
    switch (mode) {
    case AttachExternal:
    case AttachCrashedExternal:
        rc = startAttachDebugger(sp->attachPID, mode, &errorMessage);
        needWatchTimer = true; // Fetch away module load, etc. even if crashed
        break;
    case StartInternal:
    case StartExternal:
        if (sp->useTerminal) {
            // Launch console stub and wait for its startup
            m_d->m_consoleStubProc.stop(); // We leave the console open, so recycle it now.
            m_d->m_consoleStubProc.setWorkingDirectory(sp->workingDir);
            m_d->m_consoleStubProc.setEnvironment(sp->environment);
            rc = m_d->m_consoleStubProc.start(sp->executable, sp->processArgs);
            if (!rc)
                errorMessage = tr("The console stub process was unable to start '%1'.").arg(sp->executable);
            // continues in slotConsoleStubStarted()...
        } else {
            needWatchTimer = true;
            rc = startDebuggerWithExecutable(mode, &errorMessage);
        }
        break;
    case AttachCore:
        errorMessage = tr("Attaching to core files is not supported!");
        break;
    }
    if (rc) {
        m_d->m_debuggerManager->showStatusMessage(tr("Debugger running"), -1);
        if (needWatchTimer)
            startWatchTimer();
    } else {
        warning(errorMessage);
    }
    return rc;
}

bool CdbDebugEngine::startAttachDebugger(qint64 pid, DebuggerStartMode sm, QString *errorMessage)
{
    // Need to attrach invasively, otherwise, no notification signals
    // for for CreateProcess/ExitProcess occur.
    const ULONG flags = DEBUG_ATTACH_INVASIVE_RESUME_PROCESS;
    const HRESULT hr = m_d->m_cif.debugClient->AttachProcess(NULL, pid, flags);
    if (debugCDB)
        qDebug() << "Attaching to " << pid << " returns " << hr << executionStatusString(m_d->m_cif.debugControl);
    if (FAILED(hr)) {
        *errorMessage = tr("Attaching to a process failed for process id %1: %2").arg(pid).arg(msgDebugEngineComResult(hr));
        return false;
    } else {
        m_d->m_mode = sm;
    }
    return true;
}

bool CdbDebugEngine::startDebuggerWithExecutable(DebuggerStartMode sm, QString *errorMessage)
{
    m_d->m_debuggerManager->showStatusMessage("Starting Debugger", -1);

    DEBUG_CREATE_PROCESS_OPTIONS dbgopts;
    memset(&dbgopts, 0, sizeof(dbgopts));
    dbgopts.CreateFlags = DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS;

    const QSharedPointer<DebuggerStartParameters> sp = m_d->m_debuggerManager->startParameters();
    const QString filename(sp->executable);
    if (debugCDB)
        qDebug() << Q_FUNC_INFO <<filename;

    const QFileInfo fi(filename);
    m_d->m_cif.debugSymbols->AppendImagePathWide(reinterpret_cast<PCWSTR>(QDir::toNativeSeparators(fi.absolutePath()).utf16()));
    //m_cif.debugSymbols->SetSymbolOptions(SYMOPT_CASE_INSENSITIVE | SYMOPT_UNDNAME | SYMOPT_DEBUG | SYMOPT_LOAD_LINES | SYMOPT_OMAP_FIND_NEAREST | SYMOPT_AUTO_PUBLICS);
    m_d->m_cif.debugSymbols->SetSymbolOptions(SYMOPT_CASE_INSENSITIVE | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_OMAP_FIND_NEAREST | SYMOPT_AUTO_PUBLICS);
    //m_cif.debugSymbols->AddSymbolOptions(SYMOPT_CASE_INSENSITIVE | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_DEBUG | SYMOPT_LOAD_LINES | SYMOPT_OMAP_FIND_NEAREST | SYMOPT_AUTO_PUBLICS | SYMOPT_NO_IMAGE_SEARCH);

    // TODO console
    const QString cmd = Core::Utils::AbstractProcess::createWinCommandline(filename, sp->processArgs);
    if (debugCDB)
        qDebug() << "Starting " << cmd;
    PCWSTR env = 0;
    QByteArray envData;
    if (!sp->environment.empty()) {
        envData = Core::Utils::AbstractProcess::createWinEnvironment(Core::Utils::AbstractProcess::fixWinEnvironment(sp->environment));
        env = reinterpret_cast<PCWSTR>(envData.data());
    }
    const HRESULT hr = m_d->m_cif.debugClient->CreateProcess2Wide(NULL,
                                                               reinterpret_cast<PWSTR>(const_cast<ushort *>(cmd.utf16())),
                                                               &dbgopts,
                                                               sizeof(dbgopts),
                                                               reinterpret_cast<PCWSTR>(sp->workingDir.utf16()),
                                                               env);
    if (FAILED(hr)) {
        *errorMessage = tr("Unable to create a process '%1': %2").arg(cmd, msgDebugEngineComResult(hr));
        m_d->m_debuggerManagerAccess->notifyInferiorExited();
        return false;
    } else {
        m_d->m_mode = sm;
    }
    m_d->m_debuggerManagerAccess->notifyInferiorRunning();
    return true;
}

// check for a breakpoint at 'main()'
static inline bool hasBreakPointAtMain(const BreakHandler *bp)
{
    if (const int count = bp->size()) {
        // check all variations, resolved or not
        const QString main = QLatin1String("main");
        const QString qMain = QLatin1String("qMain");
        const QString moduleMainPattern = QLatin1String("!main");
        for (int i = 0; i < count ; i++) {
            const QString &function = bp->at(i)->funcName;
            if (function == main || function == qMain || function.endsWith(moduleMainPattern))
                return true;
        }
    }
    return false;
}

void CdbDebugEnginePrivate::processCreatedAttached(ULONG64 processHandle, ULONG64 initialThreadHandle)
{
    setDebuggeeHandles(reinterpret_cast<HANDLE>(processHandle), reinterpret_cast<HANDLE>(initialThreadHandle));
    m_debuggerManagerAccess->notifyInferiorRunning();
    ULONG currentThreadId;
    if (SUCCEEDED(m_cif.debugSystemObjects->GetThreadIdByHandle(initialThreadHandle, &currentThreadId))) {
        m_currentThreadId = currentThreadId;
    } else {
        m_currentThreadId = 0;
    }
    // Clear any saved breakpoints and set initial breakpoints
    m_engine->executeDebuggerCommand(QLatin1String("bc"));
    if (m_debuggerManagerAccess->breakHandler()->hasPendingBreakpoints())
        m_engine->attemptBreakpointSynchronization();
    // Attaching to crashed: This handshake (signalling an event) is required for
    // the exception to be delivered to the debugger
    if (m_mode == AttachCrashedExternal) {
        const QString crashParameter = m_debuggerManager->startParameters()->crashParameter;
        if (!crashParameter.isEmpty()) {
            ULONG64 evtNr = crashParameter.toULongLong();
            const HRESULT hr = m_cif.debugControl->SetNotifyEventHandle(evtNr);
            if (FAILED(hr))
                m_engine->warning(QString::fromLatin1("Handshake failed on event #%1: %2").arg(evtNr).arg(msgComFailed("SetNotifyEventHandle", hr)));
        }
    }
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << '\n' << executionStatusString(m_cif.debugControl);
}

void CdbDebugEngine::processTerminated(unsigned long exitCode)
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << exitCode;

    m_d->clearForRun();
    m_d->setDebuggeeHandles(0, 0);
    m_d->m_debuggerManagerAccess->notifyInferiorExited();
    m_d->m_debuggerManager->exitDebugger();
}

// End debugging using
void CdbDebugEnginePrivate::endDebugging(EndDebuggingMode em)
{
    enum Action { Detach, Terminate };
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << em;

    if (m_mode == AttachCore || !m_hDebuggeeProcess)
        return;
    // Figure out action
    Action action;
    switch (em) {
    case EndDebuggingAuto:
        action = (m_mode == AttachExternal || m_mode == AttachCrashedExternal) ?
                 Detach : Terminate;
        break;
    case EndDebuggingDetach:
        action = Detach;
        break;
    case EndDebuggingTerminate:
        action = Terminate;
        break;
    }
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << action;
    // Need a stopped debuggee to act
    QString errorMessage;
    const bool wasRunning = isDebuggeeRunning();
    if (wasRunning) { // Process must be stopped in order to terminate
        interruptInterferiorProcess(&errorMessage);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    HRESULT hr;
    switch (action) {
    case Detach:
        hr = m_cif.debugClient->DetachCurrentProcess();
        if (FAILED(hr))
            errorMessage += msgComFailed("DetachCurrentProcess", hr);
        break;
    case Terminate:
        hr = m_cif.debugClient->TerminateCurrentProcess();
        if (FAILED(hr))
            errorMessage += msgComFailed("TerminateCurrentProcess", hr);
        if (!wasRunning) {
            hr = m_cif.debugClient->TerminateProcesses();
            if (FAILED(hr))
                errorMessage += msgComFailed("TerminateProcesses", hr);
        }
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        break;
    }
    setDebuggeeHandles(0, 0);
    m_engine->killWatchTimer();

    // Clean up resources (open files, etc.)
    hr = m_cif.debugClient->EndSession(DEBUG_END_PASSIVE);
    if (FAILED(hr))
        errorMessage += msgComFailed("EndSession", hr);

    if (!errorMessage.isEmpty()) {
        errorMessage = QString::fromLatin1("There were errors trying to end debugging: %1").arg(errorMessage);
        m_debuggerManagerAccess->showDebuggerOutput(QLatin1String("error"), errorMessage);
        m_engine->warning(errorMessage);
    }
}

void CdbDebugEngine::exitDebugger()
{
    m_d->endDebugging();
}

void CdbDebugEngine::detachDebugger()
{
    m_d->endDebugging(CdbDebugEnginePrivate::EndDebuggingDetach);
}

CdbStackFrameContext *CdbDebugEnginePrivate::getStackFrameContext(int frameIndex, QString *errorMessage) const
{
    if (!m_currentStackTrace) {
        *errorMessage = QLatin1String(msgNoStackTraceC);
        return 0;
    }
    if (CdbStackFrameContext *sg = m_currentStackTrace->frameContextAt(frameIndex, errorMessage))
        return sg;
    return 0;
}

static inline QString formatWatchList(const WatchList &wl)
{
    const int count = wl.size();
    QString rc;
    for (int i = 0; i < count; i++) {
        if (i)
            rc += QLatin1String(", ");
        rc += wl.at(i).iname;
        rc += QLatin1String(" (");
        rc += wl.at(i).exp;
        rc += QLatin1Char(')');
    }
    return rc;
}

bool CdbDebugEnginePrivate::updateLocals(int frameIndex,
                                         WatchHandler *wh,
                                         QString *errorMessage)
{
    wh->reinitializeWatchers();

    QList<WatchData> incompletes = wh->takeCurrentIncompletes();
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << "\n    " << frameIndex << formatWatchList(incompletes);

    m_engine->filterEvaluateWatchers(&incompletes, wh);
    if (!incompletes.empty()) {
        const QString msg = QLatin1String("Warning: Locals left in incomplete list: ") + formatWatchList(incompletes);
        m_engine->warning(msg);
    }

    bool success = false;
    if (CdbStackFrameContext *sgc = getStackFrameContext(frameIndex, errorMessage))
        success = sgc->populateModelInitially(wh, errorMessage);

    wh->rebuildModel();
    return success;
}

void CdbDebugEngine::evaluateWatcher(WatchData *wd)
{
    if (debugCDB > 1)
        qDebug() << Q_FUNC_INFO << wd->exp;
    QString errorMessage;
    QString value;
    QString type;
    if (evaluateExpression(wd->exp, &value, &type, &errorMessage)) {
        wd->setValue(value);
        wd->setType(type);
    } else {
        wd->setValue(errorMessage);
        wd->setTypeUnneeded();
    }
    wd->setChildCount(0);
}

void CdbDebugEngine::filterEvaluateWatchers(QList<WatchData> *wd, WatchHandler *wh)
{
    typedef QList<WatchData> WatchList;
    if (wd->empty())
        return;

    // Filter out actual watchers. Ignore the "<Edit>" top level place holders
    SyntaxSetter syntaxSetter(m_d->m_cif.debugControl, DEBUG_EXPR_CPLUSPLUS);
    const QString watcherPrefix = QLatin1String("watch.");
    const QChar lessThan = QLatin1Char('<');
    const QChar greaterThan = QLatin1Char('>');
    bool placeHolderSeen = false;
    for (WatchList::iterator it = wd->begin(); it != wd->end(); ) {
        if (it->iname.startsWith(watcherPrefix)) {
            const bool isPlaceHolder = it->exp.startsWith(lessThan) && it->exp.endsWith(greaterThan);
            if (isPlaceHolder) {
                if (!placeHolderSeen) { // Max one place holder
                    it->setChildCount(0);
                    it->setAllUnneeded();
                    wh->insertData(*it);
                    placeHolderSeen = true;
                }
            } else {
                evaluateWatcher(&(*it));
                wh->insertData(*it);
            }
            it = wd->erase(it);
        } else {
            ++it;
        }
    }
}

void CdbDebugEngine::updateWatchModel()
{
    // Stack trace exists and evaluation funcs can only be called
    // when running
    if (m_d->isDebuggeeRunning()) {
        qWarning("updateWatchModel() called while debuggee is running.");
        return;
    }

    const int frameIndex = m_d->m_debuggerManagerAccess->stackHandler()->currentIndex();

    WatchHandler *watchHandler = m_d->m_debuggerManagerAccess->watchHandler();
    WatchList incomplete = watchHandler->takeCurrentIncompletes();
    if (incomplete.empty())
        return;
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << "\n    fi=" << frameIndex << formatWatchList(incomplete);

    bool success = false;
    QString errorMessage;
    do {
        // Filter out actual watchers
        filterEvaluateWatchers(&incomplete, watchHandler);
        // Do locals. We might get called while running when someone enters watchers
        if (!incomplete.empty()) {
            CdbStackFrameContext *sg = m_d->m_currentStackTrace->frameContextAt(frameIndex, &errorMessage);
            if (!sg || !sg->completeModel(incomplete, watchHandler, &errorMessage))
                break;
        }
        watchHandler->rebuildModel();
        success = true;
    } while (false);
    if (!success)
        warning(msgFunctionFailed(Q_FUNC_INFO, errorMessage));
}

void CdbDebugEngine::stepExec()
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO;

    m_d->clearForRun();
    const HRESULT hr = m_d->m_cif.debugControl->SetExecutionStatus(DEBUG_STATUS_STEP_INTO);
    if (FAILED(hr))
        warning(msgFunctionFailed(Q_FUNC_INFO, msgComFailed("SetExecutionStatus", hr)));

    m_d->m_breakEventMode = CdbDebugEnginePrivate::BreakEventIgnoreOnce;
    startWatchTimer();
}

void CdbDebugEngine::stepOutExec()
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO;

    StackHandler* sh = m_d->m_debuggerManagerAccess->stackHandler();
    const int idx = sh->currentIndex() + 1;
    QList<StackFrame> stackframes = sh->frames();
    if (idx < 0 || idx >= stackframes.size()) {
        warning(QString::fromLatin1("cannot step out"));
        return;
    }

    // Set a temporary breakpoint and continue
    const StackFrame& frame = stackframes.at(idx);
    bool success = false;
    QString errorMessage;
    do {
        const ULONG64 address = frame.address.toULongLong(&success, 16);
        if (!success) {
            errorMessage = QLatin1String("Cannot obtain address from stack frame");
            break;
        }

        IDebugBreakpoint2* pBP;
        HRESULT hr = m_d->m_cif.debugControl->AddBreakpoint2(DEBUG_BREAKPOINT_CODE, DEBUG_ANY_ID, &pBP);
        if (FAILED(hr) || !pBP) {
            errorMessage = QString::fromLatin1("Cannot create temporary breakpoint: %1").arg(msgDebugEngineComResult(hr));
            break;
        }

        pBP->SetOffset(address);
        pBP->AddFlags(DEBUG_BREAKPOINT_ENABLED);
        pBP->AddFlags(DEBUG_BREAKPOINT_ONE_SHOT);
        if (!m_d->continueInferior(&errorMessage))
            break;
        success = true;
    } while (false);
    if (!success)
        warning(msgFunctionFailed(Q_FUNC_INFO, errorMessage));
}

void CdbDebugEngine::nextExec()
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO;

    m_d->clearForRun();
    const HRESULT hr = m_d->m_cif.debugControl->SetExecutionStatus(DEBUG_STATUS_STEP_OVER);
    if (SUCCEEDED(hr)) {
        startWatchTimer();
    } else {
        warning(msgFunctionFailed(Q_FUNC_INFO, msgComFailed("SetExecutionStatus", hr)));
    }
}

void CdbDebugEngine::stepIExec()
{
    warning(QString::fromLatin1("CdbDebugEngine::stepIExec() not implemented"));
}

void CdbDebugEngine::nextIExec()
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO;

    m_d->clearForRun();
    const HRESULT hr = m_d->m_cif.debugControl->Execute(DEBUG_OUTCTL_THIS_CLIENT, "p", 0);
    if (SUCCEEDED(hr)) {
        startWatchTimer();
    } else {
       warning(msgFunctionFailed(Q_FUNC_INFO, msgDebugEngineComResult(hr)));
    }
}

void CdbDebugEngine::continueInferior()
{
    QString errorMessage;
    if  (!m_d->continueInferior(&errorMessage))
        warning(msgFunctionFailed(Q_FUNC_INFO, errorMessage));
}

// Continue process without notifications
bool CdbDebugEnginePrivate::continueInferiorProcess(QString *errorMessagePtr /* = 0 */)
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO;
    const HRESULT hr = m_cif.debugControl->SetExecutionStatus(DEBUG_STATUS_GO);
    if (FAILED(hr)) {
        const QString errorMessage = msgComFailed("SetExecutionStatus", hr);
        if (errorMessagePtr) {
            *errorMessagePtr = errorMessage;
        } else {
            m_engine->warning(msgFunctionFailed(Q_FUNC_INFO, errorMessage));
        }
        return false;
    }
    return  true;
}

// Continue process with notifications
bool CdbDebugEnginePrivate::continueInferior(QString *errorMessage)
{
    ULONG executionStatus;
    if (!getExecutionStatus(m_cif.debugControl, &executionStatus, errorMessage))
        return false;

    if (debugCDB)
        qDebug() << Q_FUNC_INFO << "\n    ex=" << executionStatus;

    if (executionStatus == DEBUG_STATUS_GO) {
        m_engine->warning(QLatin1String("continueInferior() called while debuggee is running."));
        return true;
    }

    clearForRun();
    m_engine->killWatchTimer();
    m_debuggerManager->resetLocation();
    m_debuggerManagerAccess->notifyInferiorRunningRequested();

    if (!continueInferiorProcess(errorMessage))
        return false;

    m_engine->startWatchTimer();
    m_debuggerManagerAccess->notifyInferiorRunning();
    return true;
}

bool CdbDebugEnginePrivate::interruptInterferiorProcess(QString *errorMessage)
{
    // Interrupt the interferior process without notifications
    if (debugCDB) {
        ULONG executionStatus;
        getExecutionStatus(m_cif.debugControl, &executionStatus, errorMessage);
        qDebug() << Q_FUNC_INFO << "\n    ex=" << executionStatus;
    }

    if (!DebugBreakProcess(m_hDebuggeeProcess)) {
        *errorMessage = QString::fromLatin1("DebugBreakProcess failed: %1").arg(Core::Utils::winErrorMessage(GetLastError()));
        return false;
    }
#if 0
    const HRESULT hr = m_cif.debugControl->SetInterrupt(DEBUG_INTERRUPT_ACTIVE|DEBUG_INTERRUPT_EXIT);
    if (FAILED(hr)) {
        *errorMessage = QString::fromLatin1("Unable to interrupt debuggee after %1s: %2").
                        arg(getInterruptTimeOutSecs(m_cif.debugControl)).arg(msgComFailed("SetInterrupt", hr));
        return false;
    }
#endif
    return true;
}

void CdbDebugEngine::interruptInferior()
{
    if (!m_d->m_hDebuggeeProcess || !m_d->isDebuggeeRunning())
        return;

    QString errorMessage;
    if (m_d->interruptInterferiorProcess(&errorMessage)) {
        m_d->m_debuggerManagerAccess->notifyInferiorStopped();
    } else {
        warning(msgFunctionFailed(Q_FUNC_INFO, errorMessage));
    }
}

void CdbDebugEngine::runToLineExec(const QString &fileName, int lineNumber)
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << fileName << lineNumber;
}

void CdbDebugEngine::runToFunctionExec(const QString &functionName)
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << functionName;
}

void CdbDebugEngine::jumpToLineExec(const QString &fileName, int lineNumber)
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << fileName << lineNumber;
}

void CdbDebugEngine::assignValueInDebugger(const QString &expr, const QString &value)
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << expr << value;
    const int frameIndex = m_d->m_debuggerManagerAccess->stackHandler()->currentIndex();
    QString errorMessage;
    bool success = false;
    do {
        QString newValue;
        CdbStackFrameContext *sg = m_d->getStackFrameContext(frameIndex, &errorMessage);
        if (!sg)
            break;
        if (!sg->assignValue(expr, value, &newValue, &errorMessage))
            break;
        // Update view
        WatchHandler *watchHandler = m_d->m_debuggerManagerAccess->watchHandler();
        if (WatchData *fwd = watchHandler->findData(expr)) {
            fwd->setValue(newValue);
            watchHandler->insertData(*fwd);
            watchHandler->rebuildModel();
        }
        success = true;
    } while (false);
    if (!success) {
        const QString msg = tr("Unable to assign the value '%1' to '%2': %3").arg(value, expr, errorMessage);
        warning(msg);
    }
}

void CdbDebugEngine::executeDebuggerCommand(const QString &command)
{
    QString errorMessage;
    if (!CdbDebugEnginePrivate::executeDebuggerCommand(m_d->m_cif.debugControl, command, &errorMessage))
        warning(errorMessage);
}

bool CdbDebugEnginePrivate::executeDebuggerCommand(CIDebugControl *ctrl, const QString &command, QString *errorMessage)
{
    // output to all clients, else we do not see anything
    const HRESULT hr = ctrl->ExecuteWide(DEBUG_OUTCTL_ALL_CLIENTS, reinterpret_cast<PCWSTR>(command.utf16()), 0);
    if (debugCDB)
        qDebug() << "executeDebuggerCommand" << command << SUCCEEDED(hr);
    if (FAILED(hr)) {
        *errorMessage = QString::fromLatin1("Unable to execute '%1': %2").
                        arg(command, msgDebugEngineComResult(hr));
        return false;
    }
    return true;
}

bool CdbDebugEngine::evaluateExpression(const QString &expression,
                                        QString *value,
                                        QString *type,
                                        QString *errorMessage)
{
    DEBUG_VALUE debugValue;
    if (!m_d->evaluateExpression(m_d->m_cif.debugControl, expression, &debugValue, errorMessage))
        return false;
    *value = CdbSymbolGroupContext::debugValueToString(debugValue, m_d->m_cif.debugControl, type);
    return true;
}

bool CdbDebugEnginePrivate::evaluateExpression(CIDebugControl *ctrl,
                                               const QString &expression,
                                               DEBUG_VALUE *debugValue,
                                               QString *errorMessage)
{
    if (debugCDB > 1)
        qDebug() << Q_FUNC_INFO << expression;

    memset(debugValue, 0, sizeof(DEBUG_VALUE));
    // Original syntax must be restored, else setting breakpoints will fail.
    SyntaxSetter syntaxSetter(ctrl, DEBUG_EXPR_CPLUSPLUS);
    ULONG errorPosition = 0;
    const HRESULT hr = ctrl->EvaluateWide(reinterpret_cast<PCWSTR>(expression.utf16()),
                                          DEBUG_VALUE_INVALID, debugValue,
                                          &errorPosition);
    if (FAILED(hr)) {
        if (HRESULT_CODE(hr) == 517) {
            *errorMessage = QString::fromLatin1("Unable to evaluate '%1': Expression out of scope.").
                            arg(expression);
        } else {
            *errorMessage = QString::fromLatin1("Unable to evaluate '%1': Error at %2: %3").
                            arg(expression).arg(errorPosition).arg(msgDebugEngineComResult(hr));
        }
        return false;
    }
    return true;
}

void CdbDebugEngine::activateFrame(int frameIndex)
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << frameIndex;

    if (m_d->m_debuggerManager->status() != DebuggerInferiorStopped) {
        qWarning("WARNING %s: invoked while debuggee is running\n", Q_FUNC_INFO);
        return;
    }

    QString errorMessage;
    bool success = false;
    do {
        StackHandler *stackHandler = m_d->m_debuggerManagerAccess->stackHandler();
        WatchHandler *watchHandler = m_d->m_debuggerManagerAccess->watchHandler();
        const int oldIndex = stackHandler->currentIndex();
        if (frameIndex >= stackHandler->stackSize()) {
            errorMessage = msgStackIndexOutOfRange(frameIndex, stackHandler->stackSize());
            break;
        }

        if (oldIndex != frameIndex)
            stackHandler->setCurrentIndex(frameIndex);

        const StackFrame &frame = stackHandler->currentFrame();
        if (!frame.isUsable()) {
            // Clean out model
            watchHandler->reinitializeWatchers();
            watchHandler->rebuildModel();
            errorMessage = QString::fromLatin1("%1: file %2 unusable.").
                           arg(QLatin1String(Q_FUNC_INFO), frame.file);
            break;
        }

        if (oldIndex != frameIndex || m_d->m_firstActivatedFrame)
            if (!m_d->updateLocals(frameIndex, watchHandler, &errorMessage))
                break;

        m_d->m_debuggerManager->gotoLocation(frame.file, frame.line, true);
        success =true;
    } while (false);
    if (!success)
        warning(msgFunctionFailed(Q_FUNC_INFO, errorMessage));
    m_d->m_firstActivatedFrame = false;
}

void CdbDebugEngine::selectThread(int index)
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << index;

    //reset location arrow
    m_d->m_debuggerManager->resetLocation();

    ThreadsHandler *threadsHandler = m_d->m_debuggerManagerAccess->threadsHandler();
    threadsHandler->setCurrentThread(index);
    m_d->m_currentThreadId = index;
    m_d->updateStackTrace();
}

void CdbDebugEngine::attemptBreakpointSynchronization()
{
    if (!m_d->m_hDebuggeeProcess) // Sometimes called from the breakpoint Window
        return;
    QString errorMessage;
    if (!m_d->attemptBreakpointSynchronization(&errorMessage))
        warning(msgFunctionFailed(Q_FUNC_INFO, errorMessage));
}

bool CdbDebugEnginePrivate::attemptBreakpointSynchronization(QString *errorMessage)
{
    if (!m_hDebuggeeProcess) {
        *errorMessage = QLatin1String("attemptBreakpointSynchronization() called while debugger is not running");
        return false;
    }
    // This is called from
    // 1) CreateProcessEvent with the halted engine
    // 2) from the break handler, potentially while the debuggee is running
    // If the debuggee is running (for which the execution status is
    // no reliable indicator), we temporarily halt and have ourselves
    // called again from the debug event handler.

    ULONG dummy;
    const bool wasRunning = !CDBBreakPoint::getBreakPointCount(m_cif.debugControl, &dummy);
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << "\n  Running=" << wasRunning;

    if (wasRunning) {
        const HandleBreakEventMode oldMode = m_breakEventMode;
        m_breakEventMode = BreakEventSyncBreakPoints;
        if (!interruptInterferiorProcess(errorMessage)) {
            m_breakEventMode = oldMode;
            return false;
        }
        return true;
    }

    QStringList warnings;
    const bool ok = CDBBreakPoint::synchronizeBreakPoints(m_cif.debugControl,
                                                 m_cif.debugSymbols,
                                                 m_debuggerManagerAccess->breakHandler(),
                                                 errorMessage, &warnings);
    if (const int warningsCount = warnings.size())        
        for (int w = 0; w < warningsCount; w++)
            m_engine->warning(warnings.at(w));
    return ok;
}

void CdbDebugEngine::reloadDisassembler()
{
    enum { ContextLines = 40 };
    // Do we have a top stack frame?
    const ULONG64 offset = m_d->m_currentStackTrace ? m_d->m_currentStackTrace->instructionOffset() : ULONG64(0);
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << offset;

    DisassemblerHandler *dh = m_d->m_debuggerManagerAccess->disassemblerHandler();
    if (offset) {
        QList<DisassemblerLine> lines;
        QString errorMessage;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const bool drc = dissassemble(m_d->m_cif.debugClient, m_d->m_cif.debugControl, offset,
                         ContextLines, ContextLines, &lines, &errorMessage);
        QApplication::restoreOverrideCursor();
        if (drc) {
            dh->setLines(lines);
            if (lines.size() > ContextLines)
                dh->setCurrentLine(ContextLines);
        } else {
            warning(msgFunctionFailed(Q_FUNC_INFO, errorMessage));
        }
    } else {
        dh->setLines(QList<DisassemblerLine>());
    }
}

void CdbDebugEngine::reloadModules()
{
}

void CdbDebugEngine::loadSymbols(const QString &moduleName)
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << moduleName;
}

void CdbDebugEngine::loadAllSymbols()
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO;
}

QList<Symbol> CdbDebugEngine::moduleSymbols(const QString &moduleName)
{
    QList<Symbol> rc;
    QString errorMessage;
    bool success = false;
    do {
        if (m_d->isDebuggeeRunning()) {
            errorMessage = tr("Cannot retrieve symbols while the debuggee is running.");
            break;
        }
        if (!getModuleSymbols(m_d->m_cif.debugSymbols, moduleName, &rc, &errorMessage))
            break;
        success = true;
    } while (false);
    if (!success)
        warning(errorMessage);
    return rc;
}

static inline int registerFormatBase()
{
    switch(checkedRegisterFormatAction()) {
    case FormatHexadecimal:
        return 16;
    case FormatDecimal:
        return 10;
    case FormatOctal:
        return 8;
    case FormatBinary:
        return 2;
        break;
    case FormatRaw:
    case FormatNatural:
        break;
    }
    return 10;
}

void CdbDebugEngine::reloadRegisters()
{
    const int intBase = registerFormatBase();
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << intBase;
    QList<Register> registers;
    QString errorMessage;
    if (!getRegisters(m_d->m_cif.debugControl, m_d->m_cif.debugRegisters, &registers, &errorMessage, intBase))
        warning(msgFunctionFailed("reloadRegisters" , errorMessage));
    m_d->m_debuggerManagerAccess->registerHandler()->setRegisters(registers);
}

void CdbDebugEngine::timerEvent(QTimerEvent* te)
{
    // Fetch away the debug events and notify if debuggee
    // stops. Note that IDebugEventCallback does not
    // cover all cases of a debuggee stopping execution
    // (such as step over,etc).
    if (te->timerId() != m_d->m_watchTimer)
        return;

    const HRESULT hr = m_d->m_cif.debugControl->WaitForEvent(0, 1);
    if (debugCDB)
        if (debugCDB > 1 || hr != S_FALSE)
            qDebug() << Q_FUNC_INFO << "WaitForEvent" << m_d->m_debuggerManager->status() <<   msgDebugEngineComResult(hr);

    switch (hr) {
        case S_OK:
            killWatchTimer();
            m_d->handleDebugEvent();
            break;
        case S_FALSE:
        case E_PENDING:
        case E_FAIL:
            break;
        case E_UNEXPECTED: // Occurs on ExitProcess.
            killWatchTimer();
            break;
    }
}

void CdbDebugEngine::slotConsoleStubStarted()
{
    const qint64 appPid = m_d->m_consoleStubProc.applicationPID();
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << appPid;
    // Attach to console process
    QString errorMessage;
    if (startAttachDebugger(appPid, AttachExternal, &errorMessage)) {
        startWatchTimer();
        m_d->m_debuggerManagerAccess->notifyInferiorPidChanged(appPid);
        m_d->m_debuggerManagerAccess->notifyInferiorRunning();
    } else {
        QMessageBox::critical(m_d->m_debuggerManager->mainWindow(), tr("Debugger Error"), errorMessage);
    }
}

void CdbDebugEngine::slotConsoleStubError(const QString &msg)
{
    QMessageBox::critical(m_d->m_debuggerManager->mainWindow(), tr("Debugger Error"), msg);
}

void CdbDebugEngine::slotConsoleStubTerminated()
{
    exitDebugger();
}

void CdbDebugEngine::slotAttachedCrashed()
{
 m_d->m_debuggerManagerAccess->showDebuggerOutput("A","A");
    m_d->handleDebugEvent();
}

void CdbDebugEngine::warning(const QString &w)
{
    static const QString prefix = QLatin1String("warning:");
    m_d->m_debuggerManagerAccess->showDebuggerOutput(prefix, w);
    qWarning("%s\n", qPrintable(w));
}

void CdbDebugEnginePrivate::notifyCrashed()
{
    // Cannot go over crash point to execute calls.
    m_dumper->disable();
}

void CdbDebugEnginePrivate::handleDebugEvent()
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << '\n'  << m_hDebuggeeProcess << m_breakEventMode
                << executionStatusString(m_cif.debugControl);

    // restore mode and do special handling
    const HandleBreakEventMode mode = m_breakEventMode;
    m_breakEventMode = BreakEventHandle;

    switch (mode) {
    case BreakEventHandle:
        m_debuggerManagerAccess->notifyInferiorStopped();
        updateThreadList();
        updateStackTrace();
        break;
    case BreakEventIgnoreOnce:
        m_engine->startWatchTimer();
        break;
    case BreakEventSyncBreakPoints: {
            // Temp stop to sync breakpoints
            QString errorMessage;
            attemptBreakpointSynchronization(&errorMessage);
            m_engine->startWatchTimer();
            continueInferiorProcess(&errorMessage);
            if (!errorMessage.isEmpty())
                m_engine->warning(QString::fromLatin1("In handleDebugEvent: %1").arg(errorMessage));
    }
        break;
    }
}

void CdbDebugEnginePrivate::setDebuggeeHandles(HANDLE hDebuggeeProcess,  HANDLE hDebuggeeThread)
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << hDebuggeeProcess << hDebuggeeThread;
    m_hDebuggeeProcess = hDebuggeeProcess;
    m_hDebuggeeThread = hDebuggeeThread;
}

void CdbDebugEnginePrivate::updateThreadList()
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << m_hDebuggeeProcess;

    ThreadsHandler* th = m_debuggerManagerAccess->threadsHandler();
    QList<ThreadData> threads;
    bool success = false;
    QString errorMessage;
    do {
        ULONG numberOfThreads;
        HRESULT hr= m_cif.debugSystemObjects->GetNumberThreads(&numberOfThreads);
        if (FAILED(hr)) {
            errorMessage= msgComFailed("GetNumberThreads", hr);
            break;
        }
        const ULONG maxThreadIds = 256;
        ULONG threadIds[maxThreadIds];
        ULONG biggestThreadId = qMin(maxThreadIds, numberOfThreads - 1);
        hr = m_cif.debugSystemObjects->GetThreadIdsByIndex(0, biggestThreadId, threadIds, 0);
        if (FAILED(hr)) {
            errorMessage= msgComFailed("GetThreadIdsByIndex", hr);
            break;
        }
        for (ULONG threadId = 0; threadId <= biggestThreadId; ++threadId) {
            ThreadData thread;
            thread.id = threadId;
            threads.append(thread);
        }

        th->setThreads(threads);
        success = true;
    } while (false);
    if (!success)
        m_engine->warning(msgFunctionFailed(Q_FUNC_INFO, errorMessage));
}

void CdbDebugEnginePrivate::updateStackTrace()
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO;
    // Create a new context
    clearForRun();
    QString errorMessage;
    m_engine->reloadRegisters();
    m_currentStackTrace =
            CdbStackTraceContext::create(m_dumper, m_currentThreadId, &errorMessage);
    if (!m_currentStackTrace) {
        m_engine->warning(msgFunctionFailed(Q_FUNC_INFO, errorMessage));
        return;
    }
    // Disassembling slows things down a bit. Assembler is still available via menu.
#if 0
    m_engine->reloadDisassembler(); // requires stack trace
#endif
    const QList<StackFrame> stackFrames = m_currentStackTrace->frames();
    // find the first usable frame and select it
    int current = -1;
    const int count = stackFrames.count();
    for (int i=0; i < count; ++i)
        if (stackFrames.at(i).isUsable()) {
            current = i;
            break;
        }

    m_debuggerManagerAccess->stackHandler()->setFrames(stackFrames);
    m_firstActivatedFrame = true;
    if (current >= 0) {
        m_debuggerManagerAccess->stackHandler()->setCurrentIndex(current);
        m_engine->activateFrame(current);
    }
}

void CdbDebugEnginePrivate::updateModules()
{
    QList<Module> modules;
    QString errorMessage;
    if (!getModuleList(m_cif.debugSymbols, &modules, &errorMessage))
        m_engine->warning(msgFunctionFailed(Q_FUNC_INFO, errorMessage));
    m_debuggerManagerAccess->modulesHandler()->setModules(modules);
}

static const char *dumperPrefixC = "dumper";

void CdbDebugEnginePrivate::handleModuleLoad(const QString &name)
{
    if (debugCDB>2)
        qDebug() << Q_FUNC_INFO << "\n    " << name;
    m_dumper->moduleLoadHook(name, m_hDebuggeeProcess);
    updateModules();
}

void CdbDebugEnginePrivate::handleBreakpointEvent(PDEBUG_BREAKPOINT2 pBP)
{
    Q_UNUSED(pBP)
    if (debugCDB)
        qDebug() << Q_FUNC_INFO;
}

void CdbDebugEngine::reloadSourceFiles()
{
}

QStringList CdbDebugEnginePrivate::sourcePaths() const
{
    WCHAR wszBuf[MAX_PATH];
    if (SUCCEEDED(m_cif.debugSymbols->GetSourcePathWide(wszBuf, MAX_PATH, 0)))
        return QString::fromUtf16(reinterpret_cast<const ushort *>(wszBuf)).split(QLatin1Char(';'));
    return QStringList();
}

void CdbDebugEngine::syncDebuggerPaths()
{
     if (debugCDB)
        qDebug() << Q_FUNC_INFO << m_d->m_options->symbolPaths << m_d->m_options->sourcePaths;
    QString errorMessage;
    if (!m_d->setSourcePaths(m_d->m_options->sourcePaths, &errorMessage)
        || !m_d->setSymbolPaths(m_d->m_options->symbolPaths, &errorMessage)) {
        errorMessage = QString::fromLatin1("Unable to set the debugger paths: %1").arg(errorMessage);
        warning(errorMessage);
    }
}

static inline QString pathString(const QStringList &s)
{  return s.join(QString(QLatin1Char(';')));  }

bool CdbDebugEnginePrivate::setSourcePaths(const QStringList &s, QString *errorMessage)
{
    const HRESULT hr = m_cif.debugSymbols->SetSourcePathWide(reinterpret_cast<PCWSTR>(pathString(s).utf16()));
    if (FAILED(hr)) {
        if (errorMessage)
            *errorMessage = msgComFailed("SetSourcePathWide", hr);
        return false;
    }
    return true;
}

QStringList CdbDebugEnginePrivate::symbolPaths() const
{
    WCHAR wszBuf[MAX_PATH];
    if (SUCCEEDED(m_cif.debugSymbols->GetSymbolPathWide(wszBuf, MAX_PATH, 0)))
        return QString::fromUtf16(reinterpret_cast<const ushort *>(wszBuf)).split(QLatin1Char(';'));
    return QStringList();
}

bool CdbDebugEnginePrivate::setSymbolPaths(const QStringList &s, QString *errorMessage)
{
    const HRESULT hr = m_cif.debugSymbols->SetSymbolPathWide(reinterpret_cast<PCWSTR>(pathString(s).utf16()));
    if (FAILED(hr)) {
        if (errorMessage)
            *errorMessage = msgComFailed("SetSymbolPathWide", hr);
        return false;
    }
    return true;
}

} // namespace Internal
} // namespace Debugger

// Accessed by DebuggerManager
Debugger::Internal::IDebuggerEngine *createWinEngine(Debugger::Internal::DebuggerManager *parent,
                                                     bool cmdLineEnabled,
                                                     QList<Core::IOptionsPage*> *opts)
{
    // Create options page
    QSharedPointer<Debugger::Internal::CdbOptions> options(new Debugger::Internal::CdbOptions);
    options->fromSettings(Core::ICore::instance()->settings());
    Debugger::Internal::CdbOptionsPage *optionsPage = new Debugger::Internal::CdbOptionsPage(options);
    opts->push_back(optionsPage);
    if (!cmdLineEnabled || !options->enabled)
        return 0;
    // Create engine
    QString errorMessage;
    Debugger::Internal::IDebuggerEngine *engine =
            Debugger::Internal::CdbDebugEngine::create(parent, options, &errorMessage);
    if (!engine) {
        optionsPage->setFailureMessage(errorMessage);
        qWarning("%s\n" ,qPrintable(errorMessage));
    }
    QObject::connect(optionsPage, SIGNAL(debuggerPathsChanged()), engine, SLOT(syncDebuggerPaths()));
    return engine;
}
