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

#include "profileeditor.h"

#include "profilehighlighter.h"
#include "qt4projectmanager.h"
#include "qt4projectmanagerconstants.h"
#include "profileeditorfactory.h"

#include <coreplugin/uniqueidmanager.h>
#include <texteditor/fontsettings.h>
#include <texteditor/texteditoractionhandler.h>
#include <texteditor/texteditorconstants.h>
#include <texteditor/texteditorsettings.h>

#include <QtCore/QFileInfo>

using namespace Qt4ProjectManager;
using namespace Qt4ProjectManager::Internal;

//
// ProFileEditorEditable
//

ProFileEditorEditable::ProFileEditorEditable(ProFileEditor *editor)
    : BaseTextEditorEditable(editor)
{
    Core::UniqueIDManager *uidm = Core::UniqueIDManager::instance();
    m_context << uidm->uniqueIdentifier(Qt4ProjectManager::Constants::C_PROFILEEDITOR);
    m_context << uidm->uniqueIdentifier(TextEditor::Constants::C_TEXTEDITOR);
//    m_contexts << uidm->uniqueIdentifier(Qt4ProjectManager::Constants::PROJECT_KIND);
}

QList<int> ProFileEditorEditable::context() const
{
    return m_context;
}

Core::IEditor *ProFileEditorEditable::duplicate(QWidget *parent)
{
    ProFileEditor *ret = new ProFileEditor(parent, qobject_cast<ProFileEditor*>(editor())->factory(),
                                           qobject_cast<ProFileEditor*>(editor())->actionHandler());
    ret->duplicateFrom(editor());
    TextEditor::TextEditorSettings::instance()->initializeEditor(ret);
    return ret->editableInterface();
}

const char *ProFileEditorEditable::kind() const
{
    return Qt4ProjectManager::Constants::PROFILE_EDITOR;
}

//
// ProFileEditorEditor
//

ProFileEditor::ProFileEditor(QWidget *parent, ProFileEditorFactory *factory, TextEditor::TextEditorActionHandler *ah)
    : BaseTextEditor(parent), m_factory(factory), m_ah(ah)
{
    Qt4Manager *manager = factory->qt4ProjectManager();
    ProFileDocument *doc = new ProFileDocument(manager);
    doc->setMimeType(QLatin1String(Qt4ProjectManager::Constants::PROFILE_MIMETYPE));
    setBaseTextDocument(doc);

    ah->setupActions(this);

    baseTextDocument()->setSyntaxHighlighter(new ProFileHighlighter);
}

ProFileEditor::~ProFileEditor()
{
}

TextEditor::BaseTextEditorEditable *ProFileEditor::createEditableInterface()
{
    return new ProFileEditorEditable(this);
}

void ProFileEditor::setFontSettings(const TextEditor::FontSettings &fs)
{
    TextEditor::BaseTextEditor::setFontSettings(fs);
    ProFileHighlighter *highlighter = qobject_cast<ProFileHighlighter*>(baseTextDocument()->syntaxHighlighter());
    if (!highlighter)
        return;

    static QVector<QString> categories;
    if (categories.isEmpty()) {
        categories << QLatin1String(TextEditor::Constants::C_TYPE)
                   << QLatin1String(TextEditor::Constants::C_KEYWORD)
                   << QLatin1String(TextEditor::Constants::C_COMMENT);
    }

    const QVector<QTextCharFormat> formats = fs.toTextCharFormats(categories);
    highlighter->setFormats(formats.constBegin(), formats.constEnd());
    highlighter->rehighlight();
}

//
// ProFileDocument
//

ProFileDocument::ProFileDocument(Qt4Manager *manager)
        : TextEditor::BaseTextDocument(), m_manager(manager)
{
}

bool ProFileDocument::save(const QString &name)
{
    if (BaseTextDocument::save(name)) {
        m_manager->notifyChanged(name);
        return true;
    }
    return false;
}

QString ProFileDocument::defaultPath() const
{
    QFileInfo fi(fileName());
    return fi.absolutePath();
}

QString ProFileDocument::suggestedFileName() const
{
    QFileInfo fi(fileName());
    return fi.fileName();
}
