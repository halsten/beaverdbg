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

#ifndef DEBUGGER_MODULESWINDOW_H
#define DEBUGGER_MODULESWINDOW_H

#include <QTreeView>

namespace Debugger {
namespace Internal {

class DebuggerManager;

class ModulesWindow : public QTreeView
{
    Q_OBJECT

public:
    explicit ModulesWindow(DebuggerManager *debuggerManager, QWidget *parent = 0);

signals:
    void reloadModulesRequested();
    void displaySourceRequested(const QString &modulesName);
    void loadSymbolsRequested(const QString &modulesName);
    void loadAllSymbolsRequested();
    void fileOpenRequested(QString);
    void newDockRequested(QWidget *w);

public slots:
    void resizeColumnsToContents();
    void setAlwaysResizeColumnsToContents(bool on);
    void moduleActivated(const QModelIndex &);
    void showSymbols(const QString &name);
    void setAlternatingRowColorsHelper(bool on) { setAlternatingRowColors(on); }

private:
    void resizeEvent(QResizeEvent *ev);
    void contextMenuEvent(QContextMenuEvent *ev);
    void setModel(QAbstractItemModel *model);

    bool m_alwaysResizeColumnsToContents;
    DebuggerManager *m_debuggerManager;
};

} // namespace Internal
} // namespace Debugger

#endif // DEBUGGER_MODULESWINDOW_H

