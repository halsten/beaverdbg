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

#ifndef SEARCHRESULTWINDOW_H
#define SEARCHRESULTWINDOW_H

#include "find_global.h"
#include "searchresulttreeview.h"

#include <coreplugin/ioutputpane.h>


QT_BEGIN_NAMESPACE
class QStackedWidget;
class QListWidget;
class QToolButton;
class QLabel;
QT_END_NAMESPACE

namespace Find {

class SearchResultWindow;

struct FIND_EXPORT SearchResultItem
{
    QString fileName;
    int lineNumber;
    QString lineText;
    int searchTermStart;
    int searchTermLength;
    int index;
    QVariant userData;
    // whatever information we also need here
};

class FIND_EXPORT SearchResult : public QObject
{
    Q_OBJECT

signals:
    void activated(const Find::SearchResultItem &item);
    void replaceButtonClicked(const QString &replaceText, const QList<Find::SearchResultItem> &checkedItems);

    friend class SearchResultWindow;
};

class FIND_EXPORT SearchResultWindow : public Core::IOutputPane
{
    Q_OBJECT

public:
    enum SearchMode {
        SearchOnly,
        SearchAndReplace
    };

    SearchResultWindow();
    ~SearchResultWindow();

    QWidget *outputWidget(QWidget *);
    QList<QWidget*> toolBarWidgets() const;

    QString name() const { return tr("Search Results"); }
    int priorityInStatusBar() const;
    void visibilityChanged(bool visible);
    bool isEmpty() const;
    int numberOfResults() const;
    bool hasFocus();
    bool canFocus();
    void setFocus();

    bool canNext();
    bool canPrevious();
    void goToNext();
    void goToPrev();
    bool canNavigate();

    void setTextEditorFont(const QFont &font);

    void setTextToReplace(const QString &textToReplace);
    QString textToReplace() const;

    // search result object only lives till next startnewsearch call
    SearchResult *startNewSearch(SearchMode searchOrSearchAndReplace = SearchOnly);

public slots:
    void clearContents();
    void addResult(const QString &fileName, int lineNumber, const QString &lineText,
                   int searchTermStart, int searchTermLength, const QVariant &userData = QVariant());
    void finishSearch();

private slots:
    void handleExpandCollapseToolButton(bool checked);
    void handleJumpToSearchResult(int index, bool checked);
    void handleReplaceButton();
    void showNoMatchesFound();

private:
    void setShowReplaceUI(bool show);
    void readSettings();
    void writeSettings();
    QList<SearchResultItem> checkedItems() const;

    Internal::SearchResultTreeView *m_searchResultTreeView;
    QListWidget *m_noMatchesFoundDisplay;
    QToolButton *m_expandCollapseToolButton;
    QLabel *m_replaceLabel;
    QLineEdit *m_replaceTextEdit;
    QToolButton *m_replaceButton;
    static const bool m_initiallyExpand = false;
    QStackedWidget *m_widget;
    SearchResult *m_currentSearch;
    QList<SearchResultItem> m_items;
    bool m_isShowingReplaceUI;
    bool m_focusReplaceEdit;
};

} // namespace Find

#endif // SEARCHRESULTWINDOW_H
