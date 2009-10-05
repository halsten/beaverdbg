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

#include "debuggerplugin.h"

#include "breakhandler.h"
#include "debuggeractions.h"
#include "debuggerdialogs.h"
#include "debuggerconstants.h"
#include "debuggermanager.h"
#include "debuggerrunner.h"

#include "ui_commonoptionspage.h"
#include "ui_dumperoptionpage.h"

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/basemode.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/findplaceholder.h>
#include <coreplugin/icore.h>
#include <coreplugin/messagemanager.h>
#include <coreplugin/minisplitter.h>
#include <coreplugin/modemanager.h>
#include <coreplugin/navigationwidget.h>
#include <coreplugin/outputpane.h>
#include <coreplugin/rightpane.h>
#include <coreplugin/uniqueidmanager.h>

#include <cplusplus/ExpressionUnderCursor.h>

#include <cppeditor/cppeditorconstants.h>

#include <extensionsystem/pluginmanager.h>

#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/session.h>

#include <texteditor/basetexteditor.h>
#include <texteditor/basetextmark.h>
#include <texteditor/itexteditor.h>
#include <texteditor/texteditorconstants.h>

#include <utils/qtcassert.h>

#include <QtCore/QDebug>
#include <QtCore/QObject>
#include <QtCore/QPoint>
#include <QtCore/QSettings>
#include <QtCore/QtPlugin>
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>

#include <QtGui/QLineEdit>
#include <QtGui/QDockWidget>
#include <QtGui/QMainWindow>
#include <QtGui/QPlainTextEdit>
#include <QtGui/QTextBlock>
#include <QtGui/QTextCursor>
#include <QtGui/QMessageBox>

using namespace Core;
using namespace Debugger::Constants;
using namespace Debugger::Internal;
using namespace ProjectExplorer;
using namespace TextEditor;


namespace Debugger {
namespace Constants {

const char * const M_DEBUG_START_DEBUGGING = "QtCreator.Menu.Debug.StartDebugging";

const char * const STARTEXTERNAL        = "Debugger.StartExternal";
const char * const ATTACHEXTERNAL       = "Debugger.AttachExternal";
const char * const ATTACHCORE           = "Debugger.AttachCore";
const char * const ATTACHTCF            = "Debugger.AttachTcf";
const char * const ATTACHREMOTE         = "Debugger.AttachRemote";
const char * const DETACH               = "Debugger.Detach";

const char * const RUN_TO_LINE          = "Debugger.RunToLine";
const char * const RUN_TO_FUNCTION      = "Debugger.RunToFunction";
const char * const JUMP_TO_LINE         = "Debugger.JumpToLine";
const char * const TOGGLE_BREAK         = "Debugger.ToggleBreak";
const char * const BREAK_BY_FUNCTION    = "Debugger.BreakByFunction";
const char * const BREAK_AT_MAIN        = "Debugger.BreakAtMain";
const char * const ADD_TO_WATCH         = "Debugger.AddToWatch";

#ifdef Q_WS_MAC
const char * const INTERRUPT_KEY            = "Shift+F5";
const char * const RESET_KEY                = "Ctrl+Shift+F5";
const char * const STEP_KEY                 = "F7";
const char * const STEPOUT_KEY              = "Shift+F7";
const char * const NEXT_KEY                 = "F6";
const char * const STEPI_KEY                = "Shift+F9";
const char * const NEXTI_KEY                = "Shift+F6";
const char * const REVERSE_KEY              = "";
const char * const RUN_TO_LINE_KEY          = "Shift+F8";
const char * const RUN_TO_FUNCTION_KEY      = "Ctrl+F6";
const char * const JUMP_TO_LINE_KEY         = "Alt+D,Alt+L";
const char * const TOGGLE_BREAK_KEY         = "F8";
const char * const BREAK_BY_FUNCTION_KEY    = "Alt+D,Alt+F";
const char * const BREAK_AT_MAIN_KEY        = "Alt+D,Alt+M";
const char * const ADD_TO_WATCH_KEY         = "Alt+D,Alt+W";
#else
const char * const INTERRUPT_KEY            = "Shift+F5";
const char * const RESET_KEY                = "Ctrl+Shift+F5";
const char * const STEP_KEY                 = "F11";
const char * const STEPOUT_KEY              = "Shift+F11";
const char * const NEXT_KEY                 = "F10";
const char * const STEPI_KEY                = "";
const char * const NEXTI_KEY                = "";
const char * const REVERSE_KEY              = "F12";
const char * const RUN_TO_LINE_KEY          = "";
const char * const RUN_TO_FUNCTION_KEY      = "";
const char * const JUMP_TO_LINE_KEY         = "";
const char * const TOGGLE_BREAK_KEY         = "F9";
const char * const BREAK_BY_FUNCTION_KEY    = "";
const char * const BREAK_AT_MAIN_KEY        = "";
const char * const ADD_TO_WATCH_KEY         = "Ctrl+Alt+Q";
#endif

} // namespace Constants
} // namespace Debugger


static ProjectExplorer::SessionManager *sessionManager()
{
    return ProjectExplorer::ProjectExplorerPlugin::instance()->session();
}

static QSettings *settings()
{
    return ICore::instance()->settings();
}

///////////////////////////////////////////////////////////////////////
//
// DebugMode
//
///////////////////////////////////////////////////////////////////////

namespace Debugger {
namespace Internal {

class DebugMode : public Core::BaseMode
{
    Q_OBJECT

public:
    DebugMode(QObject *parent = 0);
    ~DebugMode();
};

DebugMode::DebugMode(QObject *parent)
  : BaseMode(parent)
{
    setName(tr("Debug"));
    setUniqueModeName(Constants::MODE_DEBUG);
    setIcon(QIcon(":/fancyactionbar/images/mode_Debug.png"));
    setPriority(Constants::P_MODE_DEBUG);
}

DebugMode::~DebugMode()
{
    // Make sure the editor manager does not get deleted
    EditorManager::instance()->setParent(0);
}

} // namespace Internal
} // namespace Debugger


///////////////////////////////////////////////////////////////////////
//
// LocationMark
//
///////////////////////////////////////////////////////////////////////

namespace Debugger {
namespace Internal {

class LocationMark : public TextEditor::BaseTextMark
{
    Q_OBJECT

public:
    LocationMark(const QString &fileName, int linenumber)
        : BaseTextMark(fileName, linenumber)
    {}
    ~LocationMark();

    QIcon icon() const;
    void updateLineNumber(int /*lineNumber*/) {}
    void updateBlock(const QTextBlock & /*block*/) {}
    void removedFromEditor() {}
};

LocationMark::~LocationMark()
{
    //qDebug() << "LOCATIONMARK DESTRUCTOR";
}

QIcon LocationMark::icon() const
{
    static const QIcon icon(":/debugger/images/location.svg");
    return icon;
}

} // namespace Internal
} // namespace Debugger


///////////////////////////////////////////////////////////////////////
//
// CommonOptionsPage
//
///////////////////////////////////////////////////////////////////////

namespace Debugger {
namespace Internal {

class CommonOptionsPage : public Core::IOptionsPage
{
    Q_OBJECT

public:
    CommonOptionsPage() {}

    // IOptionsPage
    QString id() const
        { return QLatin1String(Debugger::Constants::DEBUGGER_COMMON_SETTINGS_PAGE); }
    QString trName() const
        { return QCoreApplication::translate("Debugger", Debugger::Constants::DEBUGGER_COMMON_SETTINGS_PAGE); }
    QString category() const
        { return QLatin1String(Debugger::Constants::DEBUGGER_SETTINGS_CATEGORY);  }
    QString trCategory() const
        { return QCoreApplication::translate("Debugger", Debugger::Constants::DEBUGGER_SETTINGS_CATEGORY); }

    QWidget *createPage(QWidget *parent);
    void apply() { m_group.apply(settings()); }
    void finish() { m_group.finish(); }

private:
    Ui::CommonOptionsPage m_ui;
    Core::Utils::SavedActionSet m_group;
};

QWidget *CommonOptionsPage::createPage(QWidget *parent)
{
    QWidget *w = new QWidget(parent);
    m_ui.setupUi(w);
    m_group.clear();

    m_group.insert(theDebuggerAction(ListSourceFiles),
        m_ui.checkBoxListSourceFiles);
    m_group.insert(theDebuggerAction(UseAlternatingRowColors),
        m_ui.checkBoxUseAlternatingRowColors);
    m_group.insert(theDebuggerAction(SkipKnownFrames),
        m_ui.checkBoxSkipKnownFrames);
    m_group.insert(theDebuggerAction(UseToolTips),
        m_ui.checkBoxUseToolTips);
    m_group.insert(theDebuggerAction(EnableReverseDebugging), 
        m_ui.checkBoxEnableReverseDebugging);
    m_group.insert(theDebuggerAction(MaximalStackDepth), 
        m_ui.spinBoxMaximalStackDepth);

#ifdef USE_REVERSE_DEBUGGING
    m_ui.checkBoxEnableReverseDebugging->hide();
#endif

    return w;
}

} // namespace Internal
} // namespace Debugger


///////////////////////////////////////////////////////////////////////
//
// DebuggingHelperOptionPage
//
///////////////////////////////////////////////////////////////////////

namespace Debugger {
namespace Internal {

class DebuggingHelperOptionPage : public Core::IOptionsPage
{
    Q_OBJECT

public:
    DebuggingHelperOptionPage() {}

    // IOptionsPage
    QString id() const { return QLatin1String("DebuggingHelper"); }
    QString trName() const { return tr("Debugging Helper"); }
    QString category() const { return QLatin1String(Debugger::Constants::DEBUGGER_SETTINGS_CATEGORY); }
    QString trCategory() const { return QCoreApplication::translate("Debugger", Debugger::Constants::DEBUGGER_SETTINGS_CATEGORY); }

    QWidget *createPage(QWidget *parent);
    void apply() { m_group.apply(settings()); }
    void finish() { m_group.finish(); }

private:
    Q_SLOT void updateState();

    friend class DebuggerPlugin;
    Ui::DebuggingHelperOptionPage m_ui;

    Core::Utils::SavedActionSet m_group;
};

QWidget *DebuggingHelperOptionPage::createPage(QWidget *parent)
{
    QWidget *w = new QWidget(parent);
    m_ui.setupUi(w);

    m_ui.dumperLocationChooser->setExpectedKind(Core::Utils::PathChooser::Command);
    m_ui.dumperLocationChooser->setPromptDialogTitle(tr("Choose DebuggingHelper Location"));
    m_ui.dumperLocationChooser->setInitialBrowsePathBackup(
        Core::ICore::instance()->resourcePath() + "../../lib");

    connect(m_ui.checkBoxUseDebuggingHelpers, SIGNAL(toggled(bool)),
        this, SLOT(updateState()));
    connect(m_ui.checkBoxUseCustomDebuggingHelperLocation, SIGNAL(toggled(bool)),
        this, SLOT(updateState()));

    m_group.clear();
    m_group.insert(theDebuggerAction(UseDebuggingHelpers),
        m_ui.checkBoxUseDebuggingHelpers);
    m_group.insert(theDebuggerAction(UseCustomDebuggingHelperLocation),
        m_ui.checkBoxUseCustomDebuggingHelperLocation);
    m_group.insert(theDebuggerAction(CustomDebuggingHelperLocation),
        m_ui.dumperLocationChooser);

#ifdef QT_DEBUG
    m_group.insert(theDebuggerAction(DebugDebuggingHelpers),
        m_ui.checkBoxDebugDebuggingHelpers);
#else
    m_ui.checkBoxDebugDebuggingHelpers->hide();
#endif

    m_ui.dumperLocationChooser->
        setEnabled(theDebuggerAction(UseCustomDebuggingHelperLocation)->value().toBool());

#ifndef QT_DEBUG
#if 0
    cmd = am->registerAction(m_manager->m_dumpLogAction,
        Constants::DUMP_LOG, globalcontext);
    //cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+D,Ctrl+L")));
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Shift+F11")));
    mdebug->addAction(cmd);
#endif
#endif
    updateState();

    return w;
}

void DebuggingHelperOptionPage::updateState()
{
    m_ui.checkBoxUseCustomDebuggingHelperLocation->setEnabled(
        m_ui.checkBoxUseDebuggingHelpers->isChecked());
    bool locationEnabled = m_ui.checkBoxUseDebuggingHelpers->isChecked()
         && m_ui.checkBoxUseCustomDebuggingHelperLocation->isChecked();
    m_ui.dumperLocationChooser->setEnabled(locationEnabled);
    m_ui.dumperLocationLabel->setEnabled(locationEnabled);
}

} // namespace Internal
} // namespace Debugger

///////////////////////////////////////////////////////////////////////
//
// DebuggerPlugin
//
///////////////////////////////////////////////////////////////////////

DebuggerPlugin::DebuggerPlugin()
  : m_manager(0),
    m_debugMode(0),
    m_locationMark(0),
    m_gdbRunningContext(0),
    m_cmdLineEnabledEngines(AllEngineTypes),
    m_cmdLineAttachPid(0),
    m_cmdLineWinCrashEvent(0),
    m_toggleLockedAction(0)
{}

DebuggerPlugin::~DebuggerPlugin()
{}

void DebuggerPlugin::shutdown()
{
    QTC_ASSERT(m_manager, /**/);
    if (m_manager)
        m_manager->shutdown();

    writeSettings();
    delete DebuggerSettings::instance();

    //qDebug() << "DebuggerPlugin::~DebuggerPlugin";
    removeObject(m_debugMode);

    // FIXME: when using the line below, BreakWindow etc gets deleted twice.
    // so better leak for now...
    delete m_debugMode;
    m_debugMode = 0;

    delete m_locationMark;
    m_locationMark = 0;

    delete m_manager;
    m_manager = 0;
}

static inline QString msgParameterMissing(const QString &a)
{
    return DebuggerPlugin::tr("Option '%1' is missing the parameter.").arg(a);
}

static inline QString msgInvalidNumericParameter(const QString &a, const QString &number)
{
    return DebuggerPlugin::tr("The parameter '%1' of option '%2' is not a number.").arg(number, a);
}

// Parse arguments
bool DebuggerPlugin::parseArgument(QStringList::const_iterator &it,
                                   const QStringList::const_iterator& cend,
                                   QString *errorMessage)
{
    const QString &option = *it;
    // '-debug <pid>'
    if (*it == QLatin1String("-debug")) {
        ++it;
        if (it == cend) {
            *errorMessage = msgParameterMissing(*it);
            return false;
        }
        bool ok;
        m_cmdLineAttachPid = it->toULongLong(&ok);
        if (!ok) {
            *errorMessage = msgInvalidNumericParameter(option, *it);
            return false;
        }
        return true;
    }
    // -wincrashevent <event-handle>. A handle used for
    // a handshake when attaching to a crashed Windows process.
    if (*it == QLatin1String("-wincrashevent")) {
        ++it;
        if (it == cend) {
            *errorMessage = msgParameterMissing(*it);
            return false;
        }
        bool ok;
        m_cmdLineWinCrashEvent = it->toULongLong(&ok);
        if (!ok) {
            *errorMessage = msgInvalidNumericParameter(option, *it);
            return false;
        }
        return true;
    }
    // engine disabling
    if (option == QLatin1String("-disable-cdb")) {
        m_cmdLineEnabledEngines &= ~CdbEngineType;
        return true;
    }
    if (option == QLatin1String("-disable-gdb")) {
        m_cmdLineEnabledEngines &= ~GdbEngineType;
        return true;
    }
    if (option == QLatin1String("-disable-sdb")) {
        m_cmdLineEnabledEngines &= ~ScriptEngineType;
        return true;
    }
    if (option == QLatin1String("-disable-tcf")) {
        m_cmdLineEnabledEngines &= ~TcfEngineType;
        return true;
    }

    *errorMessage = tr("Invalid debugger option: %1").arg(option);
    return false;
}

bool DebuggerPlugin::parseArguments(const QStringList &args, QString *errorMessage)
{
    const QStringList::const_iterator cend = args.constEnd();
    for (QStringList::const_iterator it = args.constBegin(); it != cend; ++it)
        if (!parseArgument(it, cend, errorMessage))
            return false;
    if (Debugger::Constants::Internal::debug)
        qDebug().nospace() << args << "engines=0x" << QString::number(m_cmdLineEnabledEngines, 16) << " pid" << m_cmdLineAttachPid << '\n';
    return true;
}

bool DebuggerPlugin::initialize(const QStringList &arguments, QString *errorMessage)
{
    // Do not fail the whole plugin if something goes wrong here
    if (!parseArguments(arguments, errorMessage)) {
        *errorMessage = tr("Error evaluating command line arguments: %1").arg(*errorMessage);
        qWarning("%s\n", qPrintable(*errorMessage));
        errorMessage->clear();
    }

    m_manager = new DebuggerManager;
    const QList<Core::IOptionsPage *> engineOptionPages = m_manager->initializeEngines(m_cmdLineEnabledEngines);

    ICore *core = ICore::instance();
    QTC_ASSERT(core, return false);

    Core::ActionManager *am = core->actionManager();
    QTC_ASSERT(am, return false);

    Core::UniqueIDManager *uidm = core->uniqueIDManager();
    QTC_ASSERT(uidm, return false);

    QList<int> globalcontext;
    globalcontext << Core::Constants::C_GLOBAL_ID;

    QList<int> cppcontext;
    cppcontext << uidm->uniqueIdentifier(ProjectExplorer::Constants::LANG_CXX);

    QList<int> debuggercontext;
    debuggercontext << uidm->uniqueIdentifier(C_GDBDEBUGGER);

    QList<int> cppeditorcontext;
    cppeditorcontext << uidm->uniqueIdentifier(CppEditor::Constants::C_CPPEDITOR);

    QList<int> texteditorcontext;
    texteditorcontext << uidm->uniqueIdentifier(TextEditor::Constants::C_TEXTEDITOR);

    m_gdbRunningContext = uidm->uniqueIdentifier(Constants::GDBRUNNING);

    //Core::ActionContainer *mcppcontext =
    //    am->actionContainer(CppEditor::Constants::M_CONTEXT);

    // External apps
    m_startExternalAction = new QAction(this);
    m_startExternalAction->setText(tr("Start and Debug External Application..."));
    connect(m_startExternalAction, SIGNAL(triggered()),
        this, SLOT(startExternalApplication()));

    m_attachExternalAction = new QAction(this);
    m_attachExternalAction->setText(tr("Attach to Running External Application..."));
    connect(m_attachExternalAction, SIGNAL(triggered()),
        this, SLOT(attachExternalApplication()));

    m_attachCoreAction = new QAction(this);
    m_attachCoreAction->setText(tr("Attach to Core..."));
    connect(m_attachCoreAction, SIGNAL(triggered()), this, SLOT(attachCore()));

    m_attachTcfAction = new QAction(this);
    m_attachTcfAction->setText(tr("Attach to Running Tcf Agent..."));
    m_attachTcfAction->setToolTip(tr("This attaches to a running "
        "'Target Communication Framework' agent."));
    connect(m_attachTcfAction, SIGNAL(triggered()),
        this, SLOT(attachRemoteTcf()));


    m_startRemoteAction = new QAction(this);
    m_startRemoteAction->setText(tr("Start and Attach to Remote Application..."));
    connect(m_startRemoteAction, SIGNAL(triggered()),
        this, SLOT(startRemoteApplication()));


    m_detachAction = new QAction(this);
    m_detachAction->setText(tr("Detach Debugger"));
    connect(m_detachAction, SIGNAL(triggered()),
        m_manager, SLOT(detachDebugger()));

    Core::ActionContainer *mdebug =
        am->actionContainer(ProjectExplorer::Constants::M_DEBUG);

    Core::ActionContainer *mstart =
        am->actionContainer(ProjectExplorer::Constants::M_DEBUG_STARTDEBUGGING);

    Core::Command *cmd = 0;
    cmd = am->registerAction(m_manager->m_continueAction,
        ProjectExplorer::Constants::DEBUG, QList<int>() << m_gdbRunningContext);
    mstart->addAction(cmd, Core::Constants::G_DEFAULT_ONE);

    cmd = am->registerAction(m_startExternalAction,
        Constants::STARTEXTERNAL, globalcontext);
    mstart->addAction(cmd, Core::Constants::G_DEFAULT_ONE);

    cmd = am->registerAction(m_attachExternalAction,
        Constants::ATTACHEXTERNAL, globalcontext);
    mstart->addAction(cmd, Core::Constants::G_DEFAULT_ONE);

    cmd = am->registerAction(m_attachCoreAction,
        Constants::ATTACHCORE, globalcontext);
    mstart->addAction(cmd, Core::Constants::G_DEFAULT_ONE);
/*
    cmd = am->registerAction(m_attachTcfAction,
        Constants::ATTACHTCF, globalcontext);
    mdebug->addAction(cmd, Core::Constants::G_DEFAULT_ONE);
*/

    cmd = am->registerAction(m_startRemoteAction,
        Constants::ATTACHREMOTE, globalcontext);
    mstart->addAction(cmd, Core::Constants::G_DEFAULT_ONE);

    cmd = am->registerAction(m_detachAction,
        Constants::DETACH, globalcontext);
    mdebug->addAction(cmd, Core::Constants::G_DEFAULT_ONE);

    cmd = am->registerAction(m_manager->m_stopAction,
        Constants::INTERRUPT, globalcontext);
    cmd->setAttribute(Core::Command::CA_UpdateText);
    cmd->setAttribute(Core::Command::CA_UpdateIcon);
    cmd->setDefaultKeySequence(QKeySequence(Constants::INTERRUPT_KEY));
    cmd->setDefaultText(tr("Stop Debugger/Interrupt Debugger"));
    mdebug->addAction(cmd, Core::Constants::G_DEFAULT_ONE);

    cmd = am->registerAction(m_manager->m_resetAction,
        Constants::RESET, globalcontext);
    cmd->setAttribute(Core::Command::CA_UpdateText);
    cmd->setDefaultKeySequence(QKeySequence(Constants::RESET_KEY));
    cmd->setDefaultText(tr("Reset Debugger"));
    //disabled mdebug->addAction(cmd, Core::Constants::G_DEFAULT_ONE);

    QAction *sep = new QAction(this);
    sep->setSeparator(true);
    cmd = am->registerAction(sep, QLatin1String("Debugger.Sep1"), globalcontext);
    mdebug->addAction(cmd);

    cmd = am->registerAction(m_manager->m_nextAction,
        Constants::NEXT, debuggercontext);
    cmd->setDefaultKeySequence(QKeySequence(Constants::NEXT_KEY));
    mdebug->addAction(cmd);

    cmd = am->registerAction(m_manager->m_stepAction,
        Constants::STEP, debuggercontext);
    cmd->setDefaultKeySequence(QKeySequence(Constants::STEP_KEY));
    mdebug->addAction(cmd);

    cmd = am->registerAction(m_manager->m_stepOutAction,
        Constants::STEPOUT, debuggercontext);
    cmd->setDefaultKeySequence(QKeySequence(Constants::STEPOUT_KEY));
    mdebug->addAction(cmd);

    cmd = am->registerAction(m_manager->m_nextIAction,
        Constants::NEXTI, debuggercontext);
    cmd->setDefaultKeySequence(QKeySequence(Constants::NEXTI_KEY));
    mdebug->addAction(cmd);

    cmd = am->registerAction(m_manager->m_stepIAction,
        Constants::STEPI, debuggercontext);
    cmd->setDefaultKeySequence(QKeySequence(Constants::STEPI_KEY));
    mdebug->addAction(cmd);

    cmd = am->registerAction(m_manager->m_runToLineAction,
        Constants::RUN_TO_LINE, debuggercontext);
    cmd->setDefaultKeySequence(QKeySequence(Constants::RUN_TO_LINE_KEY));
    mdebug->addAction(cmd);

    cmd = am->registerAction(m_manager->m_runToFunctionAction,
        Constants::RUN_TO_FUNCTION, debuggercontext);
    cmd->setDefaultKeySequence(QKeySequence(Constants::RUN_TO_FUNCTION_KEY));
    mdebug->addAction(cmd);

    cmd = am->registerAction(m_manager->m_jumpToLineAction,
        Constants::JUMP_TO_LINE, debuggercontext);
    mdebug->addAction(cmd);

#ifdef USE_REVERSE_DEBUGGING
    cmd = am->registerAction(m_manager->m_reverseDirectionAction,
        Constants::REVERSE, debuggercontext);
    cmd->setDefaultKeySequence(QKeySequence(Constants::REVERSE_KEY));
    mdebug->addAction(cmd);
#endif

    sep = new QAction(this);
    sep->setSeparator(true);
    cmd = am->registerAction(sep, QLatin1String("Debugger.Sep3"), globalcontext);
    mdebug->addAction(cmd);

    cmd = am->registerAction(m_manager->m_breakAction,
        Constants::TOGGLE_BREAK, cppeditorcontext);
    cmd->setDefaultKeySequence(QKeySequence(Constants::TOGGLE_BREAK_KEY));
    mdebug->addAction(cmd);
    //mcppcontext->addAction(cmd);

    sep = new QAction(this);
    sep->setSeparator(true);
    cmd = am->registerAction(sep, QLatin1String("Debugger.Sep2"), globalcontext);
    mdebug->addAction(cmd);

    sep = new QAction(this);
    sep->setSeparator(true);
    cmd = am->registerAction(sep, QLatin1String("Debugger.Sep4"), globalcontext);
    mdebug->addAction(cmd);

    cmd = am->registerAction(m_manager->m_watchAction,
        Constants::ADD_TO_WATCH, cppeditorcontext);
    //cmd->setDefaultKeySequence(QKeySequence(tr("ALT+D,ALT+W")));
    mdebug->addAction(cmd);

    // Views menu
    cmd = am->registerAction(sep, QLatin1String("Debugger.Sep5"), globalcontext);
    mdebug->addAction(cmd);
    ActionContainer *viewsMenu = am->createMenu(Constants::M_DEBUG_VIEWS);
    QMenu *m = viewsMenu->menu();
    m->setEnabled(true);
    m->setTitle(tr("&Views"));
    mdebug->addMenu(viewsMenu, Core::Constants::G_DEFAULT_THREE);

    m_toggleLockedAction = new QAction(tr("Locked"), this);
    m_toggleLockedAction->setCheckable(true);
    m_toggleLockedAction->setChecked(true);
    connect(m_toggleLockedAction, SIGNAL(toggled(bool)),
        m_manager, SLOT(setLocked(bool)));
    foreach (QDockWidget *dockWidget, m_manager->dockWidgets()) {
        cmd = am->registerAction(dockWidget->toggleViewAction(),
            "Debugger." + dockWidget->objectName(), debuggercontext);
        viewsMenu->addAction(cmd);
        //m->addAction(dockWidget->toggleViewAction());
    }
    m->addSeparator();
    m->addAction(m_toggleLockedAction);
    m->addSeparator();

    QAction *resetToSimpleAction = viewsMenu->menu()->addAction(tr("Reset to default layout"));
    connect(resetToSimpleAction, SIGNAL(triggered()),
        m_manager, SLOT(setSimpleDockWidgetArrangement()));

    connect(theDebuggerAction(FormatHexadecimal), SIGNAL(triggered()),
        m_manager, SLOT(reloadRegisters()));
    connect(theDebuggerAction(FormatDecimal), SIGNAL(triggered()),
        m_manager, SLOT(reloadRegisters()));
    connect(theDebuggerAction(FormatOctal), SIGNAL(triggered()),
        m_manager, SLOT(reloadRegisters()));
    connect(theDebuggerAction(FormatBinary), SIGNAL(triggered()),
        m_manager, SLOT(reloadRegisters()));
    connect(theDebuggerAction(FormatRaw), SIGNAL(triggered()),
        m_manager, SLOT(reloadRegisters()));
    connect(theDebuggerAction(FormatNatural), SIGNAL(triggered()),
        m_manager, SLOT(reloadRegisters()));

   // FIXME:
    addAutoReleasedObject(new CommonOptionsPage);
    addAutoReleasedObject(new DebuggingHelperOptionPage);
    foreach (Core::IOptionsPage* op, engineOptionPages)
        addAutoReleasedObject(op);

    m_locationMark = 0;


    //
    // Debug mode setup
    //
    m_debugMode = new DebugMode(this);
    //addAutoReleasedObject(m_debugMode);

    // register factory of DebuggerRunControl
    m_debuggerRunner = new DebuggerRunner(m_manager);
    addAutoReleasedObject(m_debuggerRunner);

    QList<int> context;
    context.append(uidm->uniqueIdentifier(Core::Constants::C_EDITORMANAGER));
    context.append(uidm->uniqueIdentifier(Debugger::Constants::C_GDBDEBUGGER));
    context.append(uidm->uniqueIdentifier(Core::Constants::C_NAVIGATION_PANE));
    m_debugMode->setContext(context);

    QBoxLayout *editorHolderLayout = new QVBoxLayout;
    editorHolderLayout->setMargin(0);
    editorHolderLayout->setSpacing(0);
    editorHolderLayout->addWidget(new EditorManagerPlaceHolder(m_debugMode));
    editorHolderLayout->addWidget(new FindToolBarPlaceHolder(m_debugMode));

    QWidget *editorAndFindWidget = new QWidget;
    editorAndFindWidget->setLayout(editorHolderLayout);

    MiniSplitter *rightPaneSplitter = new MiniSplitter;
    rightPaneSplitter->addWidget(editorAndFindWidget);
    rightPaneSplitter->addWidget(new RightPanePlaceHolder(m_debugMode));
    rightPaneSplitter->setStretchFactor(0, 1);
    rightPaneSplitter->setStretchFactor(1, 0);

    QWidget *centralWidget = new QWidget;

    m_manager->mainWindow()->setCentralWidget(centralWidget);

    MiniSplitter *splitter = new MiniSplitter;
    splitter->addWidget(m_manager->mainWindow());
    splitter->addWidget(new OutputPanePlaceHolder(m_debugMode));
    splitter->setStretchFactor(0, 10);
    splitter->setStretchFactor(1, 0);
    splitter->setOrientation(Qt::Vertical);

    MiniSplitter *splitter2 = new MiniSplitter;
    splitter2->addWidget(new NavigationWidgetPlaceHolder(m_debugMode));
    splitter2->addWidget(splitter);
    splitter2->setStretchFactor(0, 0);
    splitter2->setStretchFactor(1, 1);

    m_debugMode->setWidget(splitter2);

    QToolBar *debugToolBar = new QToolBar;
    debugToolBar->setProperty("topBorder", true);
    debugToolBar->addAction(am->command(ProjectExplorer::Constants::DEBUG)->action());
    debugToolBar->addAction(am->command(Constants::INTERRUPT)->action());
    debugToolBar->addAction(am->command(Constants::NEXT)->action());
    debugToolBar->addAction(am->command(Constants::STEP)->action());
    debugToolBar->addAction(am->command(Constants::STEPOUT)->action());
    debugToolBar->addSeparator();
    debugToolBar->addAction(am->command(Constants::STEPI)->action());
    debugToolBar->addAction(am->command(Constants::NEXTI)->action());
#ifdef USE_REVERSE_DEBUGGING
    debugToolBar->addSeparator();
    debugToolBar->addAction(am->command(Constants::REVERSE)->action());
#endif
    debugToolBar->addSeparator();
    debugToolBar->addWidget(new QLabel(tr("Threads:")));

    QComboBox *threadBox = new QComboBox;
    threadBox->setModel(m_manager->threadsModel());
    connect(threadBox, SIGNAL(activated(int)),
        m_manager->threadsWindow(), SIGNAL(threadSelected(int)));
    debugToolBar->addWidget(threadBox);
    debugToolBar->addWidget(m_manager->statusLabel());

    QWidget *stretch = new QWidget;
    stretch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    debugToolBar->addWidget(stretch);

    QBoxLayout *toolBarAddingLayout = new QVBoxLayout(centralWidget);
    toolBarAddingLayout->setMargin(0);
    toolBarAddingLayout->setSpacing(0);
    toolBarAddingLayout->addWidget(rightPaneSplitter);
    toolBarAddingLayout->addWidget(debugToolBar);

    m_manager->setSimpleDockWidgetArrangement();
    readSettings();

    connect(ModeManager::instance(), SIGNAL(currentModeChanged(Core::IMode*)),
            this, SLOT(onModeChanged(Core::IMode*)));
    m_debugMode->widget()->setFocusProxy(EditorManager::instance());
    addObject(m_debugMode);

    //
    //  Connections
    //

    // ProjectExplorer
    connect(sessionManager(), SIGNAL(sessionLoaded()),
       m_manager, SLOT(sessionLoaded()));
    connect(sessionManager(), SIGNAL(aboutToSaveSession()),
       m_manager, SLOT(aboutToSaveSession()));
    connect(sessionManager(), SIGNAL(sessionUnloaded()),
       m_manager, SLOT(sessionUnloaded()));

    // EditorManager
    QObject *editorManager = core->editorManager();
    connect(editorManager, SIGNAL(editorAboutToClose(Core::IEditor*)),
        this, SLOT(editorAboutToClose(Core::IEditor*)));
    connect(editorManager, SIGNAL(editorOpened(Core::IEditor*)),
        this, SLOT(editorOpened(Core::IEditor*)));

    // Application interaction
    connect(m_manager, SIGNAL(currentTextEditorRequested(QString*,int*,QObject**)),
        this, SLOT(queryCurrentTextEditor(QString*,int*,QObject**)));

    connect(m_manager, SIGNAL(setSessionValueRequested(QString,QVariant)),
        this, SLOT(setSessionValue(QString,QVariant)));
    connect(m_manager, SIGNAL(sessionValueRequested(QString,QVariant*)),
        this, SLOT(querySessionValue(QString,QVariant*)));

    connect(m_manager, SIGNAL(resetLocationRequested()),
        this, SLOT(resetLocation()));
    connect(m_manager, SIGNAL(gotoLocationRequested(QString,int,bool)),
        this, SLOT(gotoLocation(QString,int,bool)));
    connect(m_manager, SIGNAL(statusChanged(int)),
        this, SLOT(changeStatus(int)));
    connect(m_manager, SIGNAL(previousModeRequested()),
        this, SLOT(activatePreviousMode()));
    connect(m_manager, SIGNAL(debugModeRequested()),
        this, SLOT(activateDebugMode()));
    connect(m_manager, SIGNAL(statusChanged(int)),
        this, SLOT(updateActions(int)));


    connect(theDebuggerAction(SettingsDialog), SIGNAL(triggered()),
        this, SLOT(showSettingsDialog()));

    return true;
}

void DebuggerPlugin::extensionsInitialized()
{
    // time gdb -i mi -ex 'debuggerplugin.cpp:800' -ex r -ex q bin/qtcreator.bin
    const QByteArray env = qgetenv("QTC_DEBUGGER_TEST");
    //qDebug() << "EXTENSIONS INITIALIZED:" << env;
    if (!env.isEmpty())
        m_manager->runTest(QString::fromLocal8Bit(env));
    if (m_cmdLineAttachPid)
        QTimer::singleShot(0, this, SLOT(attachCmdLinePid()));
}

void DebuggerPlugin::attachCmdLinePid()
{
    m_manager->showStatusMessage(tr("Attaching to PID %1.").arg(m_cmdLineAttachPid));
    const QString crashParameter = m_cmdLineWinCrashEvent ? QString::number(m_cmdLineWinCrashEvent) : QString();
    attachExternalApplication(m_cmdLineAttachPid, crashParameter);
}

/*! Activates the previous mode when the current mode is the debug mode. */
void DebuggerPlugin::activatePreviousMode()
{
    Core::ModeManager *const modeManager = ICore::instance()->modeManager();

    if (modeManager->currentMode() == modeManager->mode(Constants::MODE_DEBUG)
            && !m_previousMode.isEmpty()) {
        modeManager->activateMode(m_previousMode);
        m_previousMode.clear();
    }
}

void DebuggerPlugin::activateDebugMode()
{
    ModeManager *modeManager = ModeManager::instance();
    m_previousMode = QLatin1String(modeManager->currentMode()->uniqueModeName());
    modeManager->activateMode(QLatin1String(MODE_DEBUG));
}

void DebuggerPlugin::queryCurrentTextEditor(QString *fileName, int *lineNumber, QObject **object)
{
    EditorManager *editorManager = EditorManager::instance();
    if (!editorManager)
        return;
    Core::IEditor *editor = editorManager->currentEditor();
    ITextEditor *textEditor = qobject_cast<ITextEditor*>(editor);
    if (!textEditor)
        return;
    if (fileName)
        *fileName = textEditor->file()->fileName();
    if (lineNumber)
        *lineNumber = textEditor->currentLine();
    if (object)
        *object = textEditor->widget();
}

void DebuggerPlugin::editorOpened(Core::IEditor *editor)
{
    if (ITextEditor *textEditor = qobject_cast<ITextEditor *>(editor)) {
        connect(textEditor, SIGNAL(markRequested(TextEditor::ITextEditor*,int)),
            this, SLOT(requestMark(TextEditor::ITextEditor*,int)));
        connect(editor, SIGNAL(tooltipRequested(TextEditor::ITextEditor*,QPoint,int)),
            this, SLOT(showToolTip(TextEditor::ITextEditor*,QPoint,int)));
        connect(textEditor, SIGNAL(markContextMenuRequested(TextEditor::ITextEditor*,int,QMenu*)),
            this, SLOT(requestContextMenu(TextEditor::ITextEditor*,int,QMenu*)));
    }
}

void DebuggerPlugin::editorAboutToClose(Core::IEditor *editor)
{
    if (ITextEditor *textEditor = qobject_cast<ITextEditor *>(editor)) {
        disconnect(textEditor, SIGNAL(markRequested(TextEditor::ITextEditor*,int)),
            this, SLOT(requestMark(TextEditor::ITextEditor*,int)));
        disconnect(editor, SIGNAL(tooltipRequested(TextEditor::ITextEditor*,QPoint,int)),
            this, SLOT(showToolTip(TextEditor::ITextEditor*,QPoint,int)));
        disconnect(textEditor, SIGNAL(markContextMenuRequested(TextEditor::ITextEditor*,int,QMenu*)),
            this, SLOT(requestContextMenu(TextEditor::ITextEditor*,int,QMenu*)));
    }
}

void DebuggerPlugin::requestContextMenu(TextEditor::ITextEditor *editor,
    int lineNumber, QMenu *menu)
{
    QString fileName = editor->file()->fileName();
    QString position = fileName + QString(":%1").arg(lineNumber);
    BreakpointData *data = m_manager->findBreakpoint(fileName, lineNumber);

    if (data) {
        // existing breakpoint
        QAction *act = new QAction(tr("Remove Breakpoint"), menu);
        act->setData(position);
        connect(act, SIGNAL(triggered()),
            this, SLOT(breakpointSetRemoveMarginActionTriggered()));
        menu->addAction(act);

        QAction *act2;
        if (data->enabled)
            act2 = new QAction(tr("Disable Breakpoint"), menu);
        else
            act2 = new QAction(tr("Enable Breakpoint"), menu);
        act2->setData(position);
        connect(act2, SIGNAL(triggered()),
            this, SLOT(breakpointEnableDisableMarginActionTriggered()));
        menu->addAction(act2);
    } else {
        // non-existing
        QAction *act = new QAction(tr("Set Breakpoint"), menu);
        act->setData(position);
        connect(act, SIGNAL(triggered()),
            this, SLOT(breakpointSetRemoveMarginActionTriggered()));
        menu->addAction(act);
    }
}

void DebuggerPlugin::breakpointSetRemoveMarginActionTriggered()
{
    if (QAction *act = qobject_cast<QAction *>(sender())) {
        QString str = act->data().toString();
        int pos = str.lastIndexOf(':');
        m_manager->toggleBreakpoint(str.left(pos), str.mid(pos + 1).toInt());
    }
}

void DebuggerPlugin::breakpointEnableDisableMarginActionTriggered()
{
    if (QAction *act = qobject_cast<QAction *>(sender())) {
        QString str = act->data().toString();
        int pos = str.lastIndexOf(':');
        m_manager->toggleBreakpointEnabled(str.left(pos), str.mid(pos + 1).toInt());
    }
}

void DebuggerPlugin::requestMark(TextEditor::ITextEditor *editor, int lineNumber)
{
    m_manager->toggleBreakpoint(editor->file()->fileName(), lineNumber);
}

void DebuggerPlugin::showToolTip(TextEditor::ITextEditor *editor,
    const QPoint &point, int pos)
{
    if (!theDebuggerBoolSetting(UseToolTips) || m_manager->status() == DebuggerProcessNotReady)
        return;

    m_manager->setToolTipExpression(point, editor, pos);
}

void DebuggerPlugin::setSessionValue(const QString &name, const QVariant &value)
{
    //qDebug() << "SET SESSION VALUE" << name << value;
    QTC_ASSERT(sessionManager(), return);
    sessionManager()->setValue(name, value);
}

void DebuggerPlugin::querySessionValue(const QString &name, QVariant *value)
{
    QTC_ASSERT(sessionManager(), return);
    *value = sessionManager()->value(name);
    //qDebug() << "GET SESSION VALUE: " << name << value;
}


void DebuggerPlugin::setConfigValue(const QString &name, const QVariant &value)
{
    QTC_ASSERT(m_debugMode, return);
    settings()->setValue(name, value);
}

QVariant DebuggerPlugin::configValue(const QString &name) const
{
    QTC_ASSERT(m_debugMode, return QVariant());
    return settings()->value(name);
}

void DebuggerPlugin::queryConfigValue(const QString &name, QVariant *value)
{
    QTC_ASSERT(m_debugMode, return);
    *value = settings()->value(name);
}

void DebuggerPlugin::resetLocation()
{
    //qDebug() << "RESET_LOCATION: current:"  << currentTextEditor();
    //qDebug() << "RESET_LOCATION: locations:"  << m_locationMark;
    //qDebug() << "RESET_LOCATION: stored:"  << m_locationMark->editor();
    delete m_locationMark;
    m_locationMark = 0;
}

void DebuggerPlugin::gotoLocation(const QString &fileName, int lineNumber,
    bool setMarker)
{
    TextEditor::BaseTextEditor::openEditorAt(fileName, lineNumber);
    if (setMarker) {
        resetLocation();
        m_locationMark = new LocationMark(fileName, lineNumber);
    }
}

void DebuggerPlugin::changeStatus(int status)
{
    bool startIsContinue = (status == DebuggerInferiorStopped);
    ICore *core = ICore::instance();
    if (startIsContinue) {
        core->addAdditionalContext(m_gdbRunningContext);
        core->updateContext();
    } else {
        core->removeAdditionalContext(m_gdbRunningContext);
        core->updateContext();
    }
}

void DebuggerPlugin::writeSettings() const
{
    QTC_ASSERT(m_manager, return);
    QTC_ASSERT(m_manager->mainWindow(), return);

    QSettings *s = settings();
    DebuggerSettings::instance()->writeSettings(s);
    s->beginGroup(QLatin1String("DebugMode"));
    s->setValue("State", m_manager->mainWindow()->saveState());
    s->setValue("Locked", m_toggleLockedAction->isChecked());
    s->endGroup();
}

void DebuggerPlugin::readSettings()
{
    QSettings *s = settings();
    DebuggerSettings::instance()->readSettings(s);

    QString defaultCommand("gdb");
#ifdef Q_OS_WIN
    defaultCommand.append(".exe");
#endif
    //QString defaultScript = ICore::instance()->resourcePath() +
    //    QLatin1String("/gdb/qt4macros");
    QString defaultScript;

    s->beginGroup(QLatin1String("DebugMode"));
    QByteArray ba = s->value("State", QByteArray()).toByteArray();
    m_toggleLockedAction->setChecked(s->value("Locked", true).toBool());
    s->endGroup();

    m_manager->mainWindow()->restoreState(ba);
    m_manager->setLocked(m_toggleLockedAction->isChecked());
}

void DebuggerPlugin::onModeChanged(IMode *mode)
{
     // FIXME: This one gets always called, even if switching between modes
     //        different then the debugger mode. E.g. Welcome and Help mode and
     //        also on shutdown.

    if (mode != m_debugMode) {
        if (m_manager)
            m_manager->modeVisibilityChanged(false);
        return;
    }
    if (m_manager)
        m_manager->modeVisibilityChanged(true);

    EditorManager *editorManager = EditorManager::instance();

    if (editorManager->currentEditor())
        editorManager->currentEditor()->widget()->setFocus();
}

void DebuggerPlugin::showSettingsDialog()
{
    Core::ICore::instance()->showOptionsDialog(
        QLatin1String(Debugger::Constants::DEBUGGER_SETTINGS_CATEGORY),
        QLatin1String(Debugger::Constants::DEBUGGER_COMMON_SETTINGS_PAGE));
}

static QSharedPointer<RunConfiguration> activeRunConfiguration()
{
    ProjectExplorer::Project *project =
        ProjectExplorerPlugin::instance()->currentProject();
    if (project)
        return project->activeRunConfiguration();
    return QSharedPointer<RunConfiguration>();
}

void DebuggerPlugin::startExternalApplication()
{
    const QSharedPointer<DebuggerStartParameters> sp(new DebuggerStartParameters);
    StartExternalDialog dlg(m_manager->mainWindow());
    dlg.setExecutableFile(
            configValue(_("LastExternalExecutableFile")).toString());
    dlg.setExecutableArguments(
            configValue(_("LastExternalExecutableArguments")).toString());
    if (dlg.exec() != QDialog::Accepted)
        return;

    setConfigValue(_("LastExternalExecutableFile"),
                   dlg.executableFile());
    setConfigValue(_("LastExternalExecutableArguments"),
                   dlg.executableArguments());
    sp->executable = dlg.executableFile();
    if (!dlg.executableArguments().isEmpty())
        sp->processArgs = dlg.executableArguments().split(QLatin1Char(' '));

    if (dlg.breakAtMain())
        m_manager->breakByFunctionMain();

    QSharedPointer<RunConfiguration> rc = activeRunConfiguration();
    if (RunControl *runControl = m_debuggerRunner
            ->run(rc, ProjectExplorer::Constants::DEBUGMODE, sp, StartExternal))
        runControl->start();
}

void DebuggerPlugin::attachExternalApplication()
{
    AttachExternalDialog dlg(m_manager->mainWindow());
    if (dlg.exec() == QDialog::Accepted)
        attachExternalApplication(dlg.attachPID());
}

void DebuggerPlugin::attachExternalApplication(qint64 pid, const QString &crashParameter)
{
    if (pid == 0) {
        QMessageBox::warning(m_manager->mainWindow(), tr("Warning"), tr("Cannot attach to PID 0"));
        return;
    }
    const QSharedPointer<DebuggerStartParameters> sp(new DebuggerStartParameters);
    sp->attachPID = pid;
    sp->crashParameter = crashParameter;
    const DebuggerStartMode dsm = crashParameter.isEmpty() ?  AttachExternal : AttachCrashedExternal;
    QSharedPointer<RunConfiguration> rc = activeRunConfiguration();
    if (RunControl *runControl = m_debuggerRunner
            ->run(rc, ProjectExplorer::Constants::DEBUGMODE, sp, dsm))
        runControl->start();
}

void DebuggerPlugin::attachCore()
{
    const QSharedPointer<DebuggerStartParameters> sp(new DebuggerStartParameters);
    AttachCoreDialog dlg(m_manager->mainWindow());
    dlg.setExecutableFile(
            configValue(_("LastExternalExecutableFile")).toString());
    dlg.setCoreFile(
            configValue(_("LastExternalCoreFile")).toString());
    if (dlg.exec() != QDialog::Accepted)
        return;
    setConfigValue(_("LastExternalExecutableFile"),
                   dlg.executableFile());
    setConfigValue(_("LastExternalCoreFile"),
                   dlg.coreFile());
    sp->executable = dlg.executableFile();
    sp->coreFile = dlg.coreFile();
    QSharedPointer<RunConfiguration> rc = activeRunConfiguration();
    if (RunControl *runControl = m_debuggerRunner
            ->run(rc, ProjectExplorer::Constants::DEBUGMODE, sp, AttachCore))
        runControl->start();
}

void DebuggerPlugin::startRemoteApplication()
{
    const QSharedPointer<DebuggerStartParameters> sp(new DebuggerStartParameters);
    StartRemoteDialog dlg(m_manager->mainWindow());
    QStringList arches;
    arches.append(_("i386:x86-64:intel"));
    dlg.setRemoteArchitectures(arches);
    dlg.setRemoteChannel(
            configValue(_("LastRemoteChannel")).toString());
    dlg.setRemoteArchitecture(
            configValue(_("LastRemoteArchitecture")).toString());
    dlg.setServerStartScript(
            configValue(_("LastServerStartScript")).toString());
    dlg.setUseServerStartScript(
            configValue(_("LastUseServerStartScript")).toBool());
    if (dlg.exec() != QDialog::Accepted)
        return;
    setConfigValue(_("LastRemoteChannel"), dlg.remoteChannel());
    setConfigValue(_("LastRemoteArchitecture"), dlg.remoteArchitecture());
    setConfigValue(_("LastServerStartScript"), dlg.serverStartScript());
    setConfigValue(_("LastUseServerStartScript"), dlg.useServerStartScript());
    sp->remoteChannel = dlg.remoteChannel();
    sp->remoteArchitecture = dlg.remoteArchitecture();
    if (dlg.useServerStartScript())
        sp->serverStartScript = dlg.serverStartScript();

    QSharedPointer<RunConfiguration> rc = activeRunConfiguration();
    if (RunControl *runControl = m_debuggerRunner
            ->run(rc, ProjectExplorer::Constants::DEBUGMODE, sp, StartRemote))
        runControl->start();
}

void DebuggerPlugin::attachRemoteTcf()
{
    const QSharedPointer<DebuggerStartParameters> sp(new DebuggerStartParameters);
    AttachTcfDialog dlg(m_manager->mainWindow());
    QStringList arches;
    arches.append(_("i386:x86-64:intel"));
    dlg.setRemoteArchitectures(arches);
    dlg.setRemoteChannel(
            configValue(_("LastTcfRemoteChannel")).toString());
    dlg.setRemoteArchitecture(
            configValue(_("LastTcfRemoteArchitecture")).toString());
    dlg.setServerStartScript(
            configValue(_("LastTcfServerStartScript")).toString());
    dlg.setUseServerStartScript(
            configValue(_("LastTcfUseServerStartScript")).toBool());
    if (dlg.exec() != QDialog::Accepted)
        return;
    setConfigValue(_("LastTcfRemoteChannel"), dlg.remoteChannel());
    setConfigValue(_("LastTcfRemoteArchitecture"), dlg.remoteArchitecture());
    setConfigValue(_("LastTcfServerStartScript"), dlg.serverStartScript());
    setConfigValue(_("LastTcfUseServerStartScript"), dlg.useServerStartScript());
    sp->remoteChannel = dlg.remoteChannel();
    sp->remoteArchitecture = dlg.remoteArchitecture();
    sp->serverStartScript = dlg.serverStartScript();
    if (dlg.useServerStartScript())
        sp->serverStartScript = dlg.serverStartScript();

    QSharedPointer<RunConfiguration> rc = activeRunConfiguration();
    if (RunControl *runControl = m_debuggerRunner
            ->run(rc, ProjectExplorer::Constants::DEBUGMODE, sp, AttachTcf))
        runControl->start();
}

void DebuggerPlugin::updateActions(int status)
{
    const bool started = status == DebuggerInferiorRunning
        || status == DebuggerInferiorRunningRequested
        || status == DebuggerInferiorStopRequested
        || status == DebuggerInferiorStopped;

    const bool starting = status == DebuggerProcessStartingUp;
    //const bool running = status == DebuggerInferiorRunning;

    const bool ready = status == DebuggerInferiorStopped
            && m_manager->startMode() != AttachCore;
    m_startExternalAction->setEnabled(!started && !starting);
    m_attachExternalAction->setEnabled(!started && !starting);
#ifdef Q_OS_WIN
    m_attachCoreAction->setEnabled(false);
#else
    m_attachCoreAction->setEnabled(!started && !starting);
#endif
    m_startRemoteAction->setEnabled(!started && !starting);
    m_detachAction->setEnabled(ready);
}

#include "debuggerplugin.moc"

Q_EXPORT_PLUGIN(DebuggerPlugin)
