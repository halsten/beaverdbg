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

#include "breakwindow.h"

#include "debuggeractions.h"
#include "ui_breakcondition.h"
#include "ui_breakbyfunction.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QFileInfoList>

#include <QtGui/QAction>
#include <QtGui/QHeaderView>
#include <QtGui/QKeyEvent>
#include <QtGui/QMenu>
#include <QtGui/QResizeEvent>
#include <QtGui/QToolButton>
#include <QtGui/QTreeView>

using Debugger::Internal::BreakWindow;


///////////////////////////////////////////////////////////////////////
//
// BreakByFunctionDialog
//
///////////////////////////////////////////////////////////////////////

class BreakByFunctionDialog : public QDialog, Ui::BreakByFunctionDialog
{
public:
    explicit BreakByFunctionDialog(QWidget *parent)
      : QDialog(parent)
    {
        setupUi(this);
        connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
        connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    }
    QString functionName() const { return functionLineEdit->text(); }
};


///////////////////////////////////////////////////////////////////////
//
// BreakWindow
//
///////////////////////////////////////////////////////////////////////

BreakWindow::BreakWindow(QWidget *parent)
  : QTreeView(parent), m_alwaysResizeColumnsToContents(false)
{
    QAction *act = theDebuggerAction(UseAlternatingRowColors);
    setWindowTitle(tr("Breakpoints"));
    setWindowIcon(QIcon(":/debugger/images/debugger_breakpoints.png"));
    setAlternatingRowColors(act->isChecked());
    setRootIsDecorated(false);
    setIconSize(QSize(10, 10));

    connect(this, SIGNAL(activated(QModelIndex)),
        this, SLOT(rowActivated(QModelIndex)));
    connect(act, SIGNAL(toggled(bool)),
        this, SLOT(setAlternatingRowColorsHelper(bool)));
}

void BreakWindow::keyPressEvent(QKeyEvent *ev)
{
    if (ev->key() == Qt::Key_Delete)
        deleteBreakpoint(currentIndex());
    QTreeView::keyPressEvent(ev);
}

void BreakWindow::resizeEvent(QResizeEvent *ev)
{
    QTreeView::resizeEvent(ev);
}

void BreakWindow::contextMenuEvent(QContextMenuEvent *ev)
{
    QMenu menu;
    const QModelIndex index = indexAt(ev->pos());
    const bool indexIsValid = index.isValid();
    const QModelIndex index0 = index.sibling(index.row(), 0);
    const QModelIndex index2 = index.sibling(index.row(), 2);
    QAction *act0 = new QAction(tr("Delete breakpoint"), &menu);
    act0->setEnabled(indexIsValid);
    QAction *act1 = new QAction(tr("Adjust column widths to contents"), &menu);
    QAction *act2 = new QAction(tr("Always adjust column widths to contents"), &menu);
    act2->setCheckable(true);
    act2->setChecked(m_alwaysResizeColumnsToContents);
    QAction *act3 = new QAction(tr("Edit condition..."), &menu);
    act3->setEnabled(indexIsValid);    
    QAction *act4 = new QAction(tr("Synchronize breakpoints"), &menu);
    bool enabled = indexIsValid && model()->data(index0, Qt::UserRole).toBool();
    QString str = enabled ? tr("Disable breakpoint") : tr("Enable breakpoint");
    QAction *act5 = new QAction(str, &menu);
    bool fullpath = indexIsValid && model()->data(index2, Qt::UserRole).toBool();
    QString str6 = fullpath ? tr("Use short path") : tr("Use full path");
    QAction *act6 = new QAction(str6, &menu);

    QAction *act7 = new QAction(this);
    act7->setText(tr("Set Breakpoint at Function..."));
    QAction *act8 = new QAction(this);
    act8->setText(tr("Set Breakpoint at Function \"main\""));


    menu.addAction(act0);
    menu.addAction(act3);
    menu.addAction(act5);
    menu.addAction(act6);
    menu.addSeparator();
    menu.addAction(act1);
    menu.addAction(act2);
    menu.addAction(act4);
    menu.addSeparator();
    menu.addAction(act7);
    menu.addAction(act8);
    menu.addSeparator();
    menu.addAction(theDebuggerAction(SettingsDialog));

    QAction *act = menu.exec(ev->globalPos());

    if (act == act0)
        deleteBreakpoint(index);
    else if (act == act1)
        resizeColumnsToContents();
    else if (act == act2)
        setAlwaysResizeColumnsToContents(!m_alwaysResizeColumnsToContents);
    else if (act == act3)
        editCondition(index);
    else if (act == act4)
        emit breakpointSynchronizationRequested();
    else if (act == act5) {
        model()->setData(index0, !enabled);
        emit breakpointSynchronizationRequested();
    } else if (act == act6) {
        model()->setData(index2, !fullpath);
        emit breakpointSynchronizationRequested();
    } else if (act == act7) {
        BreakByFunctionDialog dlg(this);
        if (dlg.exec())
            emit breakByFunctionRequested(dlg.functionName());
    } else if (act == act8)
        emit breakByFunctionMainRequested();
}

void BreakWindow::deleteBreakpoint(const QModelIndex &idx)
{
    int row = idx.row();
    if (row == model()->rowCount() - 1)
        --row;
    setCurrentIndex(idx.sibling(row, 0));
    emit breakpointDeleted(idx.row());
}

void BreakWindow::editCondition(const QModelIndex &idx)
{
    QDialog dlg(this);
    Ui::BreakCondition ui;
    ui.setupUi(&dlg);

    int row = idx.row();
    dlg.setWindowTitle(tr("Conditions on Breakpoint %1").arg(row));
    ui.lineEditCondition->setText(model()->data(idx.sibling(row, 4)).toString());
    ui.spinBoxIgnoreCount->setValue(model()->data(idx.sibling(row, 5)).toInt());

    if (dlg.exec() == QDialog::Rejected)
        return;

    model()->setData(idx.sibling(row, 4), ui.lineEditCondition->text());
    model()->setData(idx.sibling(row, 5), ui.spinBoxIgnoreCount->value());
}

void BreakWindow::resizeColumnsToContents()
{
    resizeColumnToContents(0);
    resizeColumnToContents(1);
    resizeColumnToContents(2);
    resizeColumnToContents(3);
}

void BreakWindow::setAlwaysResizeColumnsToContents(bool on)
{
    m_alwaysResizeColumnsToContents = on;
    QHeaderView::ResizeMode mode = on 
        ? QHeaderView::ResizeToContents : QHeaderView::Interactive;
    header()->setResizeMode(0, mode);
    header()->setResizeMode(1, mode);
    header()->setResizeMode(2, mode);
    header()->setResizeMode(3, mode);
}

void BreakWindow::rowActivated(const QModelIndex &index)
{
    emit breakpointActivated(index.row());
}

