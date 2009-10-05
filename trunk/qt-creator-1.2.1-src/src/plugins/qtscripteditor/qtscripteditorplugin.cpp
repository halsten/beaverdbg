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

#include "qtscripteditorplugin.h"

#include "qscripthighlighter.h"
#include "qtscripteditor.h"
#include "qtscripteditorconstants.h"
#include "qtscripteditorfactory.h"
#include "qtscriptcodecompletion.h"

#include <coreplugin/icore.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/mimedatabase.h>
#include <coreplugin/uniqueidmanager.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <extensionsystem/pluginmanager.h>
#include <texteditor/fontsettings.h>
#include <texteditor/storagesettings.h>
#include <texteditor/texteditorconstants.h>
#include <texteditor/texteditorsettings.h>
#include <texteditor/textfilewizard.h>
#include <texteditor/texteditoractionhandler.h>
#include <texteditor/completionsupport.h>
#include <utils/qtcassert.h>

#include <QtCore/QtPlugin>
#include <QtCore/QDebug>
#include <QtCore/QSettings>
#include <QtGui/QAction>

using namespace QtScriptEditor::Internal;
using namespace QtScriptEditor::Constants;

QtScriptEditorPlugin *QtScriptEditorPlugin::m_instance = 0;

QtScriptEditorPlugin::QtScriptEditorPlugin() :
    m_wizard(0),
    m_editor(0),
    m_actionHandler(0),
    m_completion(0)
{
    m_instance = this;
}

QtScriptEditorPlugin::~QtScriptEditorPlugin()
{
    removeObject(m_editor);
    removeObject(m_wizard);
    delete m_actionHandler;
    m_instance = 0;
}

bool QtScriptEditorPlugin::initialize(const QStringList & /*arguments*/, QString *error_message)
{
    typedef SharedTools::QScriptHighlighter QScriptHighlighter;

    Core::ICore *core = Core::ICore::instance();
    if (!core->mimeDatabase()->addMimeTypes(QLatin1String(":/qtscripteditor/QtScriptEditor.mimetypes.xml"), error_message))
        return false;
    m_scriptcontext << core->uniqueIDManager()->uniqueIdentifier(QtScriptEditor::Constants::C_QTSCRIPTEDITOR);
    m_context = m_scriptcontext;
    m_context << core->uniqueIDManager()->uniqueIdentifier(TextEditor::Constants::C_TEXTEDITOR);

    registerActions();

    m_editor = new QtScriptEditorFactory(m_context, this);
    addObject(m_editor);

    Core::BaseFileWizardParameters wizardParameters(Core::IWizard::FileWizard);
    wizardParameters.setDescription(tr("Creates a Qt Script file."));
    wizardParameters.setName(tr("Qt Script file"));
    wizardParameters.setCategory(QLatin1String("Qt"));
    wizardParameters.setTrCategory(tr("Qt"));
    m_wizard = new TextEditor::TextFileWizard(QLatin1String(QtScriptEditor::Constants::C_QTSCRIPTEDITOR_MIMETYPE),
                                              QLatin1String(QtScriptEditor::Constants::C_QTSCRIPTEDITOR),
                                              QLatin1String("qtscript$"),
                                              wizardParameters, this);
    addObject(m_wizard);

    m_actionHandler = new TextEditor::TextEditorActionHandler(QtScriptEditor::Constants::C_QTSCRIPTEDITOR,
          TextEditor::TextEditorActionHandler::Format
        | TextEditor::TextEditorActionHandler::UnCommentSelection
        | TextEditor::TextEditorActionHandler::UnCollapseAll);

    m_completion = new QtScriptCodeCompletion();
    addAutoReleasedObject(m_completion);

    // Restore settings
    QSettings *settings = Core::ICore::instance()->settings();
    settings->beginGroup(QLatin1String("CppTools")); // ### FIXME:
    settings->beginGroup(QLatin1String("Completion"));
    const bool caseSensitive = settings->value(QLatin1String("CaseSensitive"), true).toBool();
    m_completion->setCaseSensitivity(caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
    settings->endGroup();
    settings->endGroup();
    
    error_message->clear();

    return true;
}

void QtScriptEditorPlugin::extensionsInitialized()
{
}

void QtScriptEditorPlugin::initializeEditor(QtScriptEditor::Internal::ScriptEditor *editor)
{
    QTC_ASSERT(m_instance, /**/);

    m_actionHandler->setupActions(editor);

    TextEditor::TextEditorSettings::instance()->initializeEditor(editor);

    // auto completion
    connect(editor, SIGNAL(requestAutoCompletion(ITextEditable*, bool)),
            TextEditor::Internal::CompletionSupport::instance(), SLOT(autoComplete(ITextEditable*, bool)));
}

void QtScriptEditorPlugin::registerActions()
{
    Core::ActionManager *am = Core::ICore::instance()->actionManager();
    Core::ActionContainer *mcontext = am->createMenu(QtScriptEditor::Constants::M_CONTEXT);

    QAction *action = new QAction(this);
    action->setSeparator(true);
    Core::Command *cmd = am->registerAction(action, QtScriptEditor::Constants::RUN_SEP, m_scriptcontext);
    mcontext->addAction(cmd, Core::Constants::G_DEFAULT_THREE);

    action = new QAction(tr("Run"), this);
    cmd = am->registerAction(action, QtScriptEditor::Constants::RUN, m_scriptcontext);
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+R")));
    mcontext->addAction(cmd, Core::Constants::G_DEFAULT_THREE);
}

Q_EXPORT_PLUGIN(QtScriptEditorPlugin)
