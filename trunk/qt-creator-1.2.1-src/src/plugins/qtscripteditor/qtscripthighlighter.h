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

#ifndef QTSCRIPTSYNTAXHIGHLIGHTER_H
#define QTSCRIPTSYNTAXHIGHLIGHTER_H

#include "qscripthighlighter.h"
#include <texteditor/basetexteditor.h>

namespace QtScriptEditor {
namespace Internal {

// Highlighter for Scripts that stores
// the parentheses encountered in the block data
// for parentheses matching to work.

class QtScriptHighlighter : public SharedTools::QScriptHighlighter
{
    Q_OBJECT
public:
    QtScriptHighlighter(QTextDocument *parent = 0);

private:
    virtual int onBlockStart();
    virtual void onOpeningParenthesis(QChar parenthesis, int pos);
    virtual void onClosingParenthesis(QChar parenthesis, int pos);
    virtual void onBlockEnd(int state, int firstNonSpace);

    typedef TextEditor::Parenthesis Parenthesis;
    typedef TextEditor::Parentheses Parentheses;
    Parentheses m_currentBlockParentheses;
    int m_braceDepth;
};

} // namespace Internal
} // namespace QtScriptEditor

#endif // QTSCRIPTSYNTAXHIGHLIGHTER_H
