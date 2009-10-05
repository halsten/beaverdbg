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

#include "helpfindsupport.h"
#include "helpviewer.h"

#include <utils/qtcassert.h>

using namespace Help::Internal;

HelpFindSupport::HelpFindSupport(CentralWidget *centralWidget)
    : m_centralWidget(centralWidget)
{
}

HelpFindSupport::~HelpFindSupport()
{
}

bool HelpFindSupport::isEnabled() const
{
    return true;
}

QString HelpFindSupport::currentFindString() const
{
    QTC_ASSERT(m_centralWidget, return QString());
    HelpViewer *viewer = m_centralWidget->currentHelpViewer();
    if (!viewer)
        return QString();
#if !defined(QT_NO_WEBKIT)
    return viewer->selectedText();
#else
    return viewer->textCursor().selectedText();
#endif
}

QString HelpFindSupport::completedFindString() const
{
    return QString();
}

bool HelpFindSupport::findIncremental(const QString &txt, Find::IFindSupport::FindFlags findFlags)
{
    QTC_ASSERT(m_centralWidget, return false);
    findFlags &= ~Find::IFindSupport::FindBackward;
    return m_centralWidget->find(txt, Find::IFindSupport::textDocumentFlagsForFindFlags(findFlags), true);
}

bool HelpFindSupport::findStep(const QString &txt, Find::IFindSupport::FindFlags findFlags)
{
    QTC_ASSERT(m_centralWidget, return false);
    return m_centralWidget->find(txt, Find::IFindSupport::textDocumentFlagsForFindFlags(findFlags), false);
}

HelpViewerFindSupport::HelpViewerFindSupport(HelpViewer *viewer)
        : m_viewer(viewer)
{
}

QString HelpViewerFindSupport::currentFindString() const
{
    QTC_ASSERT(m_viewer, return QString());
#if !defined(QT_NO_WEBKIT)
    return m_viewer->selectedText();
#else
    return QString();
#endif
}

bool HelpViewerFindSupport::findIncremental(const QString &txt, Find::IFindSupport::FindFlags findFlags)
{
    QTC_ASSERT(m_viewer, return false);
    findFlags &= ~Find::IFindSupport::FindBackward;
    return find(txt, findFlags, true);
}

bool HelpViewerFindSupport::findStep(const QString &txt, Find::IFindSupport::FindFlags findFlags)
{
    QTC_ASSERT(m_viewer, return false);
    return find(txt, findFlags, false);
}

bool HelpViewerFindSupport::find(const QString &txt, Find::IFindSupport::FindFlags findFlags, bool incremental)
{
    QTC_ASSERT(m_viewer, return false);
#if !defined(QT_NO_WEBKIT)
    Q_UNUSED(incremental);
    QWebPage::FindFlags options = QWebPage::FindWrapsAroundDocument;
    if (findFlags & Find::IFindSupport::FindBackward)
        options |= QWebPage::FindBackward;
    if (findFlags & Find::IFindSupport::FindCaseSensitively)
        options |= QWebPage::FindCaseSensitively;

    return m_viewer->findText(txt, options);
#else
    QTextCursor cursor = m_viewer->textCursor();
    QTextDocument *doc = m_viewer->document();
    QTextBrowser *browser = qobject_cast<QTextBrowser*>(m_viewer);

    if (!browser || !doc || cursor.isNull())
        return false;
    if (incremental)
        cursor.setPosition(cursor.selectionStart());

    QTextCursor found = doc->find(txt, cursor, Find::IFindSupport::textDocumentFlagsForFindFlags(findFlags));
    if (found.isNull()) {
        if ((findFlags&Find::IFindSupport::FindBackward) == 0)
            cursor.movePosition(QTextCursor::Start);
        else
            cursor.movePosition(QTextCursor::End);
        found = doc->find(txt, cursor, Find::IFindSupport::textDocumentFlagsForFindFlags(findFlags));
        if (found.isNull()) {
            return false;
        }
    }
    if (!found.isNull()) {
        m_viewer->setTextCursor(found);
    }
    return true;
#endif
}
