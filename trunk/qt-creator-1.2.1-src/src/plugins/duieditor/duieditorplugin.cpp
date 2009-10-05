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

#include "duieditorplugin.h"

#include "qscripthighlighter.h"
#include "duieditor.h"
#include "duieditorconstants.h"
#include "duieditorfactory.h"
#include "duicodecompletion.h"
#include "duihoverhandler.h"

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

using namespace DuiEditor::Internal;
using namespace DuiEditor::Constants;

DuiEditorPlugin *DuiEditorPlugin::m_instance = 0;

DuiEditorPlugin::DuiEditorPlugin() :
    m_wizard(0),
    m_editor(0),
    m_actionHandler(0),
    m_completion(0)
{
    m_instance = this;
}

DuiEditorPlugin::~DuiEditorPlugin()
{
    removeObject(m_editor);
    removeObject(m_wizard);
    delete m_actionHandler;
    m_instance = 0;
}

bool DuiEditorPlugin::initialize(const QStringList & /*arguments*/, QString *error_message)
{
    typedef SharedTools::QScriptHighlighter QScriptHighlighter;

    Core::ICore *core = Core::ICore::instance();
    if (!core->mimeDatabase()->addMimeTypes(QLatin1String(":/duieditor/DuiEditor.mimetypes.xml"), error_message))
        return false;
    m_scriptcontext << core->uniqueIDManager()->uniqueIdentifier(DuiEditor::Constants::C_DUIEDITOR);
    m_context = m_scriptcontext;
    m_context << core->uniqueIDManager()->uniqueIdentifier(TextEditor::Constants::C_TEXTEDITOR);

    registerActions();

    m_editor = new DuiEditorFactory(m_context, this);
    addObject(m_editor);

    Core::BaseFileWizardParameters wizardParameters(Core::IWizard::FileWizard);
    wizardParameters.setDescription(tr("Creates a Qt QML file."));
    wizardParameters.setName(tr("Qt QML File"));
    wizardParameters.setCategory(QLatin1String("Qt"));
    wizardParameters.setTrCategory(tr("Qt"));
    m_wizard = new TextEditor::TextFileWizard(QLatin1String(DuiEditor::Constants::C_DUIEDITOR_MIMETYPE),
                                              QLatin1String(DuiEditor::Constants::C_DUIEDITOR),
                                              QLatin1String("dui$"),
                                              wizardParameters, this);
    addObject(m_wizard);

    m_actionHandler = new TextEditor::TextEditorActionHandler(DuiEditor::Constants::C_DUIEDITOR,
          TextEditor::TextEditorActionHandler::Format
        | TextEditor::TextEditorActionHandler::UnCommentSelection
        | TextEditor::TextEditorActionHandler::UnCollapseAll);

    m_completion = new DuiCodeCompletion();
    addAutoReleasedObject(m_completion);

    addAutoReleasedObject(new DuiHoverHandler());

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

void DuiEditorPlugin::extensionsInitialized()
{
}

void DuiEditorPlugin::initializeEditor(DuiEditor::Internal::ScriptEditor *editor)
{
    QTC_ASSERT(m_instance, /**/);

    m_actionHandler->setupActions(editor);

    TextEditor::TextEditorSettings::instance()->initializeEditor(editor);

    // auto completion
    connect(editor, SIGNAL(requestAutoCompletion(ITextEditable*, bool)),
            TextEditor::Internal::CompletionSupport::instance(), SLOT(autoComplete(ITextEditable*, bool)));
}

void DuiEditorPlugin::registerActions()
{
}

Q_EXPORT_PLUGIN(DuiEditorPlugin)
