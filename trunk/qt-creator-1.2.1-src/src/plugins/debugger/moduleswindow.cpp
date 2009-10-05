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

#include "moduleswindow.h"

#include "moduleshandler.h" // for model roles
#include "debuggeractions.h"
#include "debuggermanager.h"

#include <QtCore/QDebug>
#include <QtCore/QProcess>
#include <QtCore/QRegExp>

#include <QtGui/QAction>
#include <QtGui/QHeaderView>
#include <QtGui/QMenu>
#include <QtGui/QResizeEvent>
#include <QtGui/QToolButton>
#include <QtGui/QTreeWidget>
#include <QtGui/QApplication>

///////////////////////////////////////////////////////////////////////////
//
// ModulesWindow
//
///////////////////////////////////////////////////////////////////////////

namespace Debugger {
namespace Internal {

ModulesWindow::ModulesWindow(DebuggerManager *debuggerManager,
                             QWidget *parent)  :
    QTreeView(parent),
    m_alwaysResizeColumnsToContents(false),
    m_debuggerManager(debuggerManager)
{
    QAction *act = theDebuggerAction(UseAlternatingRowColors);
    setWindowTitle(tr("Modules"));
    setSortingEnabled(true);
    setAlternatingRowColors(act->isChecked());
    setRootIsDecorated(false);
    setIconSize(QSize(10, 10));

    connect(this, SIGNAL(activated(QModelIndex)),
        this, SLOT(moduleActivated(QModelIndex)));
    connect(act, SIGNAL(toggled(bool)),
        this, SLOT(setAlternatingRowColorsHelper(bool)));
}

void ModulesWindow::moduleActivated(const QModelIndex &index)
{
    qDebug() << "ACTIVATED: " << index.row() << index.column()
        << model()->data(index);
    emit fileOpenRequested(model()->data(index).toString());
}

void ModulesWindow::resizeEvent(QResizeEvent *event)
{
    //QHeaderView *hv = header();
    //int totalSize = event->size().width() - 110;
    //hv->resizeSection(0, totalSize / 4);
    //hv->resizeSection(1, totalSize / 4);
    //hv->resizeSection(2, totalSize / 4);
    //hv->resizeSection(3, totalSize / 4);
    //hv->resizeSection(0, 60);
    //hv->resizeSection(1, (totalSize * 50) / 100);
    //hv->resizeSection(2, (totalSize * 50) / 100);
    //hv->resizeSection(3, 50);
    //setColumnHidden(3, true);
    QTreeView::resizeEvent(event);
}

void ModulesWindow::contextMenuEvent(QContextMenuEvent *ev)
{
    QString name;
    QModelIndex index = indexAt(ev->pos());
    if (index.isValid())
        index = index.sibling(index.row(), 0);
    if (index.isValid())
        name = model()->data(index).toString();

    QMenu menu;
    QAction *act0 = new QAction(tr("Update module list"), &menu);
    QAction *act1 = new QAction(tr("Adjust column widths to contents"), &menu);
    QAction *act2 = new QAction(tr("Always adjust column widths to contents"), &menu);
    act2->setCheckable(true);
    act2->setChecked(m_alwaysResizeColumnsToContents);
    QAction *act3 = new QAction(tr("Show source files for module \"%1\"").arg(name),
         &menu);
    QAction *act4 = new QAction(tr("Load symbols for all modules"), &menu);
    QAction *act5 = 0;
    QAction *act6 = 0;
    QAction *act7 = 0;
    if (name.isEmpty()) {
        act5 = new QAction(tr("Load symbols for module"), &menu);
        act6 = new QAction(tr("Edit file"), &menu);
        act7 = new QAction(tr("Show symbols"), &menu);
    } else {
        act5 = new QAction(tr("Load symbols for module \"%1\"").arg(name), &menu);
        act6 = new QAction(tr("Edit file \"%1\"").arg(name), &menu);
        act7 = new QAction(tr("Show symbols in file \"%1\"").arg(name), &menu);
    }
    act5->setDisabled(name.isEmpty());
    act6->setDisabled(name.isEmpty());
    act7->setDisabled(name.isEmpty());

    menu.addAction(act0);
    menu.addAction(act4);
    menu.addAction(act5);
    menu.addAction(act6);
    menu.addAction(act7);
    menu.addSeparator();
    menu.addAction(act1);
    menu.addAction(act2);

    QAction *act = menu.exec(ev->globalPos());

    if (act == act0)
        emit reloadModulesRequested();
    else if (act == act1)
        resizeColumnsToContents();
    else if (act == act2)
        setAlwaysResizeColumnsToContents(!m_alwaysResizeColumnsToContents);
    else if (act == act3)
        emit displaySourceRequested(name);
    else if (act == act4)
        emit loadAllSymbolsRequested();
    else if (act == act5)
        emit loadSymbolsRequested(name);
    else if (act == act6)
        emit fileOpenRequested(name);
    else if (act == act7)
        showSymbols(name);
}

void ModulesWindow::resizeColumnsToContents()
{
    resizeColumnToContents(0);
    resizeColumnToContents(1);
    resizeColumnToContents(2);
}

void ModulesWindow::setAlwaysResizeColumnsToContents(bool on)
{
    m_alwaysResizeColumnsToContents = on;
    QHeaderView::ResizeMode mode = on 
        ? QHeaderView::ResizeToContents : QHeaderView::Interactive;
    header()->setResizeMode(0, mode);
    header()->setResizeMode(1, mode);
    header()->setResizeMode(2, mode);
    header()->setResizeMode(3, mode);
    //setColumnHidden(3, true);
}

void ModulesWindow::setModel(QAbstractItemModel *model)
{
    QTreeView::setModel(model);
    setAlwaysResizeColumnsToContents(true);
}
    
void ModulesWindow::showSymbols(const QString &name)
{
    if (name.isEmpty())
        return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QList<Symbol> symbols = m_debuggerManager->moduleSymbols(name);
    QApplication::restoreOverrideCursor();
    if (symbols.empty())
        return;
    QTreeWidget *w = new QTreeWidget;
    w->setColumnCount(3);
    w->setRootIsDecorated(false);
    w->setAlternatingRowColors(true);
    w->setHeaderLabels(QStringList() << tr("Address") << tr("Code") << tr("Symbol"));
    w->setWindowTitle(tr("Symbols in \"%1\"").arg(name));
    foreach (const Symbol &s, symbols) {
        QTreeWidgetItem *it = new QTreeWidgetItem;
        it->setData(0, Qt::DisplayRole, s.address);
        it->setData(1, Qt::DisplayRole, s.state);
        it->setData(2, Qt::DisplayRole, s.name);
        w->addTopLevelItem(it);
    }
    emit newDockRequested(w);
}

} // namespace Internal
} // namespace Debugger
