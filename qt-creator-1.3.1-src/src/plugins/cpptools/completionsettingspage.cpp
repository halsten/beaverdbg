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

#include "completionsettingspage.h"
#include "cppcodecompletion.h"
#include "ui_completionsettingspage.h"

#include <coreplugin/icore.h>
#include <extensionsystem/pluginmanager.h>

using namespace CppTools::Internal;

CompletionSettingsPage::CompletionSettingsPage(CppCodeCompletion *completion)
    : m_completion(completion)
    , m_page(new Ui_CompletionSettingsPage)
{
}

CompletionSettingsPage::~CompletionSettingsPage()
{
    delete m_page;
}

QString CompletionSettingsPage::id() const
{
    return QLatin1String("Completion");
}

QString CompletionSettingsPage::trName() const
{
    return tr("Completion");
}

QString CompletionSettingsPage::category() const
{
    return QLatin1String("TextEditor");
}

QString CompletionSettingsPage::trCategory() const
{
    return tr("Text Editor");
}

QWidget *CompletionSettingsPage::createPage(QWidget *parent)
{
    QWidget *w = new QWidget(parent);
    m_page->setupUi(w);

    m_page->caseSensitive->setChecked(m_completion->caseSensitivity() == Qt::CaseSensitive);
    m_page->autoInsertBrackets->setChecked(m_completion->autoInsertBrackets());
    m_page->partiallyComplete->setChecked(m_completion->isPartialCompletionEnabled());

    return w;
}

void CompletionSettingsPage::apply()
{
    m_completion->setCaseSensitivity(
            m_page->caseSensitive->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive);
    m_completion->setAutoInsertBrackets(m_page->autoInsertBrackets->isChecked());
    m_completion->setPartialCompletionEnabled(m_page->partiallyComplete->isChecked());
}
