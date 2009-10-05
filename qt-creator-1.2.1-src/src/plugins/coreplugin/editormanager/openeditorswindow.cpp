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

#include "openeditorswindow.h"
#include "editormanager.h"
#include "editorview.h"

#include <QtGui/QHeaderView>

Q_DECLARE_METATYPE(Core::IEditor *)

using namespace Core;
using namespace Core::Internal;

const int OpenEditorsWindow::WIDTH = 300;
const int OpenEditorsWindow::HEIGHT = 200;
const int OpenEditorsWindow::MARGIN = 4;

OpenEditorsWindow::OpenEditorsWindow(QWidget *parent) :
    QWidget(parent, Qt::Popup),
    m_editorList(new QTreeWidget(this))
{
    resize(QSize(WIDTH, HEIGHT));
    m_editorList->setColumnCount(1);
    m_editorList->header()->hide();
    m_editorList->setIndentation(0);
    m_editorList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_editorList->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_editorList->setTextElideMode(Qt::ElideMiddle);
#ifdef Q_WS_MAC
    m_editorList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
#endif
    m_editorList->installEventFilter(this);
    m_editorList->setGeometry(MARGIN, MARGIN, WIDTH-2*MARGIN, HEIGHT-2*MARGIN);

    connect(m_editorList, SIGNAL(itemClicked(QTreeWidgetItem*, int)),
            this, SLOT(editorClicked(QTreeWidgetItem*)));

    m_autoHide.setSingleShot(true);
    connect(&m_autoHide, SIGNAL(timeout()), this, SLOT(selectAndHide()));
}

void OpenEditorsWindow::selectAndHide()
{
    selectEditor(m_editorList->currentItem());
    setVisible(false);
}

void OpenEditorsWindow::setVisible(bool visible)
{
    QWidget::setVisible(visible);
    if (visible) {
        m_autoHide.start(600);
        setFocus();
    }
}

bool OpenEditorsWindow::isCentering()
{
    int internalMargin = m_editorList->viewport()->mapTo(m_editorList, QPoint(0,0)).y();
    QRect rect0 = m_editorList->visualItemRect(m_editorList->topLevelItem(0));
    QRect rect1 = m_editorList->visualItemRect(m_editorList->topLevelItem(m_editorList->topLevelItemCount()-1));
    int height = rect1.y() + rect1.height() - rect0.y();
    height += 2*internalMargin + 2*MARGIN;
    if (height > HEIGHT)
        return true;
    return false;
}


bool OpenEditorsWindow::event(QEvent *e) {
    if (e->type() == QEvent::KeyRelease) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(e);
        m_autoHide.stop();
        if (ke->modifiers() == 0
            /*HACK this is to overcome some event inconsistencies between platforms*/
            || (ke->modifiers() == Qt::AltModifier && (ke->key() == Qt::Key_Alt || ke->key() == -1))) {
            selectAndHide();
        }
    }
    return QWidget::event(e);
}

bool OpenEditorsWindow::eventFilter(QObject *obj, QEvent *e)
{
    if (obj == m_editorList) {
        if (e->type() == QEvent::KeyPress) {
            QKeyEvent *ke = static_cast<QKeyEvent*>(e);
            if (ke->key() == Qt::Key_Escape) {
                setVisible(false);
                return true;
            }
            if (ke->key() == Qt::Key_Return) {
                selectEditor(m_editorList->currentItem());
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, e);
}

void OpenEditorsWindow::focusInEvent(QFocusEvent *)
{
    m_editorList->setFocus();
}

void OpenEditorsWindow::selectUpDown(bool up)
{
    int itemCount = m_editorList->topLevelItemCount();
    if (itemCount < 2)
        return;
    int index = m_editorList->indexOfTopLevelItem(m_editorList->currentItem());
    if (index < 0)
        return;
    QTreeWidgetItem *editor = 0;
    int count = 0;
    while (!editor && count < itemCount) {
        if (up) {
            index--;
            if (index < 0)
                index = itemCount-1;
        } else {
            index++;
            if (index >= itemCount)
                index = 0;
        }
        editor = m_editorList->topLevelItem(index);
        count++;
    }
    if (editor) {
        m_editorList->setCurrentItem(editor);
        ensureCurrentVisible();
    }
}

void OpenEditorsWindow::selectPreviousEditor()
{
    selectUpDown(false);
}

void OpenEditorsWindow::selectNextEditor()
{
    selectUpDown(true);
}

void OpenEditorsWindow::centerOnItem(int selectedIndex)
{
    if (selectedIndex >= 0) {
        QTreeWidgetItem *item;
        int num = m_editorList->topLevelItemCount();
        int rotate = selectedIndex-(num-1)/2;
        for (int i = 0; i < rotate; ++i) {
            item = m_editorList->takeTopLevelItem(0);
            m_editorList->addTopLevelItem(item);
        }
        rotate = -rotate;
        for (int i = 0; i < rotate; ++i) {
            item = m_editorList->takeTopLevelItem(num-1);
            m_editorList->insertTopLevelItem(0, item);
        }
    }
}

void OpenEditorsWindow::setEditors(const QList<IEditor *>&editors, IEditor *current, EditorModel *model)
{
    static const QIcon lockedIcon(QLatin1String(":/core/images/locked.png"));
    static const QIcon emptyIcon(QLatin1String(":/core/images/empty14.png"));

    m_editorList->clear();

    QList<IEditor *> doneList;
    QList<IFile *>doneFileList;
    foreach (IEditor *editor, editors) {
        if (doneList.contains(editor))
            continue;
        doneList += editor;
        if (!editor->widget()->isVisible() && doneFileList.contains(editor->file()))
            continue;
        doneFileList += editor->file();

        QTreeWidgetItem *item = new QTreeWidgetItem();

        QString title = editor->displayName();
        if (editor->file()->isModified())
            title += tr("*");
        item->setIcon(0, editor->file()->isReadOnly() ? lockedIcon : emptyIcon);
        item->setText(0, title);
        item->setToolTip(0, editor->file()->fileName());
        item->setData(0, Qt::UserRole, QVariant::fromValue(editor));
        item->setTextAlignment(0, Qt::AlignLeft);

        m_editorList->addTopLevelItem(item);

        if (editor == current)
            m_editorList->setCurrentItem(item);

    }

    // add purely restored editors which are not initialised yet
    foreach (EditorModel::Entry entry, model->entries()) {
        if (entry.editor)
            continue;
        QTreeWidgetItem *item = new QTreeWidgetItem();
        QString title = entry.displayName();
        item->setIcon(0, emptyIcon);
        item->setText(0, title);
        item->setToolTip(0, entry.fileName());
        item->setData(0, Qt::UserRole+1, QVariant::fromValue(entry.kind()));
        item->setTextAlignment(0, Qt::AlignLeft);

        m_editorList->addTopLevelItem(item);
    }
}


void OpenEditorsWindow::selectEditor(QTreeWidgetItem *item)
{
    if (!item)
        return;
    if (IEditor *editor = item->data(0, Qt::UserRole).value<IEditor*>())
        EditorManager::instance()->activateEditor(editor);
    else
        EditorManager::instance()->openEditor(item->toolTip(0), item->data(0, Qt::UserRole+1).toByteArray());
}

void OpenEditorsWindow::editorClicked(QTreeWidgetItem *item)
{
    selectEditor(item);
    setFocus();
}


void OpenEditorsWindow::ensureCurrentVisible()
{
    m_editorList->scrollTo(m_editorList->currentIndex(), QAbstractItemView::PositionAtCenter);
}

