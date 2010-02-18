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

#ifndef QTSCRIPTEDITORPLUGIN_H
#define QTSCRIPTEDITORPLUGIN_H

#include <extensionsystem/iplugin.h>

namespace TextEditor {
class TextFileWizard;
class TextEditorActionHandler;
} // namespace TextEditor

namespace QtScriptEditor {
namespace Internal {

class QtScriptEditorFactory;
class QtScriptCodeCompletion;
class ScriptEditor;

class QtScriptEditorPlugin : public ExtensionSystem::IPlugin
{
    Q_OBJECT

public:
    QtScriptEditorPlugin();
    virtual ~QtScriptEditorPlugin();

    // IPlugin
    bool initialize(const QStringList &arguments, QString *errorMessage = 0);
    void extensionsInitialized();

    static QtScriptEditorPlugin *instance()
    { return m_instance; }

    void initializeEditor(ScriptEditor *editor);

private:
    void registerActions();

    static QtScriptEditorPlugin *m_instance;

    typedef QList<int> Context;
    Context m_context;
    Context m_scriptcontext;

    TextEditor::TextFileWizard *m_wizard;
    QtScriptEditorFactory *m_editor;
    TextEditor::TextEditorActionHandler *m_actionHandler;
    QtScriptCodeCompletion *m_completion;
};

} // namespace Internal
} // namespace QtScriptEditor

#endif // QTSCRIPTEDITORPLUGIN_H
