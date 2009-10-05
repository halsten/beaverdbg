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

#include "duihoverhandler.h"
#include "duieditor.h"

#include <coreplugin/icore.h>
#include <coreplugin/uniqueidmanager.h>
#include <coreplugin/editormanager/editormanager.h>
#include <extensionsystem/pluginmanager.h>
#include <texteditor/itexteditor.h>
#include <texteditor/basetexteditor.h>
#include <debugger/debuggerconstants.h>

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>
#include <QtGui/QToolTip>
#include <QtGui/QTextCursor>
#include <QtGui/QTextBlock>
#include <QtHelp/QHelpEngineCore>

using namespace DuiEditor::Internal;
using namespace Core;

DuiHoverHandler::DuiHoverHandler(QObject *parent)
    : QObject(parent)
{
    ICore *core = ICore::instance();

    // Listen for editor opened events in order to connect to tooltip/helpid requests
    connect(core->editorManager(), SIGNAL(editorOpened(Core::IEditor *)),
            this, SLOT(editorOpened(Core::IEditor *)));
}

void DuiHoverHandler::editorOpened(IEditor *editor)
{
    ScriptEditorEditable *duiEditor = qobject_cast<ScriptEditorEditable *>(editor);
    if (!duiEditor)
        return;

    connect(duiEditor, SIGNAL(tooltipRequested(TextEditor::ITextEditor*, QPoint, int)),
            this, SLOT(showToolTip(TextEditor::ITextEditor*, QPoint, int)));

    connect(duiEditor, SIGNAL(contextHelpIdRequested(TextEditor::ITextEditor*, int)),
            this, SLOT(updateContextHelpId(TextEditor::ITextEditor*, int)));
}

void DuiHoverHandler::showToolTip(TextEditor::ITextEditor *editor, const QPoint &point, int pos)
{
    if (! editor)
        return;

    ScriptEditor *ed = qobject_cast<ScriptEditor *>(editor->widget());

    ICore *core = ICore::instance();
    const int dbgcontext = core->uniqueIDManager()->uniqueIdentifier(Debugger::Constants::C_GDBDEBUGGER);

    if (core->hasContext(dbgcontext))
        return;

    m_toolTip.clear();

    QTextCursor tc = ed->textCursor();
    tc.setPosition(pos);
    const unsigned line = tc.block().blockNumber() + 1;

    foreach (const JavaScript::DiagnosticMessage &m, ed->diagnosticMessages()) {
        if (m.loc.startLine == line) {
            m_toolTip.append(m.message);
            break;
        }
    }

    if (m_toolTip.isEmpty())
        QToolTip::hideText();
    else {
        const QPoint pnt = point - QPoint(0,
#ifdef Q_WS_WIN
        24
#else
        16
#endif
        );

        QToolTip::showText(pnt, m_toolTip);
    }
}

void DuiHoverHandler::updateContextHelpId(TextEditor::ITextEditor *, int)
{
}

