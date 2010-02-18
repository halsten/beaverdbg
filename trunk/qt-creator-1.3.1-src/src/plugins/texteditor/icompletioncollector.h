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

#ifndef COMPLETIONCOLLECTORINTERFACE_H
#define COMPLETIONCOLLECTORINTERFACE_H

#include "texteditor_global.h"

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtGui/QIcon>

namespace TextEditor {

class ICompletionCollector;
class ITextEditable;

struct CompletionItem
{
    CompletionItem(ICompletionCollector *collector = 0)
        : relevance(0),
          duplicateCount(0),
          collector(collector)
    { }

    bool isValid() const
    { return collector != 0; }

    operator bool() const
    { return collector != 0; }

    QString text;
    QString details;
    QIcon icon;
    QVariant data;
    int relevance;
    int duplicateCount;
    ICompletionCollector *collector;
};

/* Defines the interface to completion collectors. A completion collector tells
 * the completion support code when a completion is triggered and provides the
 * list of possible completions. It keeps an internal state so that it can be
 * polled for the list of completions, which is reset with a call to reset.
 */
class TEXTEDITOR_EXPORT ICompletionCollector : public QObject
{
    Q_OBJECT
public:
    ICompletionCollector(QObject *parent = 0) : QObject(parent) {}
    virtual ~ICompletionCollector() {}

    /*
     * Returns true if this completion collector can be used with the given editor.
     */
    virtual bool supportsEditor(ITextEditable *editor) = 0;

    /* This method should return whether the cursor is at a position which could
     * trigger an autocomplete. It will be called each time a character is typed in
     * the text editor.
     */
    virtual bool triggersCompletion(ITextEditable *editor) = 0;

    // returns starting position
    virtual int startCompletion(ITextEditable *editor) = 0;

    /* This method should add all the completions it wants to show into the list,
     * based on the given cursor position.
     */
    virtual void completions(QList<CompletionItem> *completions) = 0;

    /* This method should complete the given completion item.
     */
    virtual void complete(const CompletionItem &item) = 0;

    /* This method gives the completion collector a chance to partially complete
     * based on a set of items. The general use case is to complete the common
     * prefix shared by all possible completion items.
     *
     * Returns whether the completion popup should be closed.
     */
    virtual bool partiallyComplete(const QList<TextEditor::CompletionItem> &completionItems) = 0;

    /* Called when it's safe to clean up the completion items.
     */
    virtual void cleanup() = 0;
};

class TEXTEDITOR_EXPORT IQuickFixCollector : public ICompletionCollector
{
    Q_OBJECT

public:
    IQuickFixCollector(QObject *parent = 0) : ICompletionCollector(parent) {}
    virtual ~IQuickFixCollector() {}

    virtual bool triggersCompletion(TextEditor::ITextEditable *)
    { return false; }

    virtual bool partiallyComplete(const QList<TextEditor::CompletionItem> &)
    { return false; }
};


} // namespace TextEditor

#endif // COMPLETIONCOLLECTORINTERFACE_H
