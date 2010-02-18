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

#ifndef DEBUGGERCONSTANTS_H
#define DEBUGGERCONSTANTS_H

#include "debugger_global.h"

namespace Debugger {
namespace Constants {

// modes and their priorities
const char * const MODE_DEBUG           = "Debugger.Mode.Debug";
const int          P_MODE_DEBUG         = 85;

// common actions
const char * const INTERRUPT            = "Debugger.Interrupt";
const char * const RESET                = "Debugger.Reset";
const char * const STEP                 = "Debugger.StepLine";
const char * const STEPOUT              = "Debugger.StepOut";
const char * const NEXT                 = "Debugger.NextLine";
const char * const REVERSE              = "Debugger.ReverseDirection";

const char * const M_DEBUG_VIEWS        = "Debugger.Menu.View.Debug";

const char * const C_GDBDEBUGGER        = "Gdb Debugger";
const char * const GDBRUNNING           = "Gdb.Running";

const char * const DEBUGGER_COMMON_SETTINGS_PAGE = QT_TRANSLATE_NOOP("Debugger", "Common");
const char * const DEBUGGER_SETTINGS_CATEGORY = QT_TRANSLATE_NOOP("Debugger", "Debugger");

namespace Internal {
    enum { debug = 0 };
#ifdef Q_OS_MAC
    const char * const LD_PRELOAD_ENV_VAR = "DYLD_INSERT_LIBRARIES";
#else
    const char * const LD_PRELOAD_ENV_VAR = "LD_PRELOAD";
#endif

}
} // namespace Constants

enum DebuggerState
{
    DebuggerNotReady,          // Debugger not started

    EngineStarting,            // Engine starts

    AdapterStarting,
    AdapterStarted,
    AdapterStartFailed,
    InferiorUnrunnable,         // Used in the core dump adapter
    InferiorStarting,
    // InferiorStarted,         // Use InferiorRunningRequested or InferiorStopped
    InferiorStartFailed,

    InferiorRunningRequested,   // Debuggee requested to run
    InferiorRunningRequested_Kill, // Debuggee requested to run, but want to kill it
    InferiorRunning,            // Debuggee running

    InferiorStopping,           // Debuggee running, stop requested
    InferiorStopping_Kill,      // Debuggee running, stop requested, want to kill it
    InferiorStopped,            // Debuggee stopped
    InferiorStopFailed,         // Debuggee not stopped, will kill debugger

    InferiorShuttingDown,
    InferiorShutDown,
    InferiorShutdownFailed,

    EngineShuttingDown
};

enum DebuggerStartMode
{
    NoStartMode,
    StartInternal,         // Start current start project's binary
    StartExternal,         // Start binary found in file system
    AttachExternal,        // Attach to running process by process id
    AttachCrashedExternal, // Attach to crashed process by process id
    AttachCore,            // Attach to a core file
    StartRemote            // Start and attach to a remote process
};

enum LogChannel
{
    LogInput,               // Used for user input
    LogOutput,
    LogWarning,
    LogError,
    LogStatus,              // Used for status changed messages
    LogTime,                // Used for time stamp messages
    LogDebug,
    LogMisc    
};

} // namespace Debugger

#endif // DEBUGGERCONSTANTS_H

