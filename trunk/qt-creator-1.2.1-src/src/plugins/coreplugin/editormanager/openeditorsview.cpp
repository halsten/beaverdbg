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

#include "openeditorsview.h"
#include "editormanager.h"
#include "editorview.h"
#include "icore.h"

#include <coreplugin/coreconstants.h>
#include <coreplugin/filemanager.h>
#include <coreplugin/uniqueidmanager.h>
#include <utils/qtcassert.h>

#include <QtCore/QTimer>
#include <QtGui/QMenu>
#include <QtGui/QPainter>
#include <QtGui/QStyle>
#include <QtGui/QStyleOption>
#include <QtGui/QHeaderView>
#include <QtGui/QKeyEvent>
#ifdef Q_WS_MAC
#include <qmacstyle_mac.h>
#endif

Q_DECLARE_METATYPE(Core::IEditor*)

using namespace Core;
using namespace Core::Internal;


OpenEditorsDelegate::OpenEditorsDelegate(QObject *parent)
 : QStyledItemDelegate(parent)
{
}

void OpenEditorsDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
           const QModelIndex &index) const
{
    if (option.state & QStyle::State_MouseOver) {
        if ((QApplication::mouseButtons() & Qt::LeftButton) == 0)
            pressedIndex = QModelIndex();
        QBrush brush = option.palette.alternateBase();
        if (index == pressedIndex)
            brush = option.palette.dark();
        painter->fillRect(option.rect, brush);
    }


    QStyledItemDelegate::paint(painter, option, index);

    if (index.column() == 1 && option.state & QStyle::State_MouseOver) {
        QIcon icon((option.state & QStyle::State_Selected) ? ":/core/images/closebutton.png"
               : ":/core/images/darkclosebutton.png");

        QRect iconRect(option.rect.right() - option.rect.height(),
                       option.rect.top(),
                       option.rect.height(),
                       option.rect.height());

        icon.paint(painter, iconRect, Qt::AlignRight | Qt::AlignVCenter);
    }

}

OpenEditorsWidget::OpenEditorsWidget()
{
    m_ui.setupUi(this);
    setWindowTitle(tr("Open Documents"));
    setWindowIcon(QIcon(Constants::ICON_DIR));
    setFocusProxy(m_ui.editorList);
    m_ui.editorList->setFocusPolicy(Qt::NoFocus);
    m_ui.editorList->viewport()->setAttribute(Qt::WA_Hover);
    m_ui.editorList->setItemDelegate((m_delegate = new OpenEditorsDelegate(this)));
    m_ui.editorList->header()->hide();
    m_ui.editorList->setIndentation(0);
    m_ui.editorList->setTextElideMode(Qt::ElideMiddle);
    m_ui.editorList->setFrameStyle(QFrame::NoFrame);
    m_ui.editorList->setAttribute(Qt::WA_MacShowFocusRect, false);
    EditorManager *em = EditorManager::instance();
    m_ui.editorList->setModel(em->openedEditorsModel());
    m_ui.editorList->setSelectionMode(QAbstractItemView::NoSelection);
    m_ui.editorList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ui.editorList->header()->setStretchLastSection(false);
    m_ui.editorList->header()->setResizeMode(0, QHeaderView::Stretch);
    m_ui.editorList->header()->setResizeMode(1, QHeaderView::Fixed);
    m_ui.editorList->header()->resizeSection(1, 16);
    connect(em, SIGNAL(currentEditorChanged(Core::IEditor*)),
            this, SLOT(updateCurrentItem(Core::IEditor*)));
    connect(m_ui.editorList, SIGNAL(clicked(QModelIndex)),
            this, SLOT(handleClicked(QModelIndex)));
    connect(m_ui.editorList, SIGNAL(pressed(QModelIndex)),
            this, SLOT(handlePressed(QModelIndex)));
}

OpenEditorsWidget::~OpenEditorsWidget()
{
}

void OpenEditorsWidget::updateCurrentItem(Core::IEditor *editor)
{
    if (!editor) {
        m_ui.editorList->clearSelection();
        return;
    }
    EditorManager *em = EditorManager::instance();
    m_ui.editorList->setCurrentIndex(em->openedEditorsModel()->indexOf(editor));
    m_ui.editorList->selectionModel()->select(m_ui.editorList->currentIndex(),
                                              QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_ui.editorList->scrollTo(m_ui.editorList->currentIndex());
}

void OpenEditorsWidget::handlePressed(const QModelIndex &index)
{
    if (index.column() == 0) {
        m_ui.editorList->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        EditorManager::instance()->activateEditor(index);
    } else if (index.column() == 1) {
        m_delegate->pressedIndex = index;
    }
}

void OpenEditorsWidget::handleClicked(const QModelIndex &index)
{
    if (index.column() == 1) { // the funky close button
        EditorManager::instance()->closeEditor(index);

        // work around a bug in itemviews where the delegate wouldn't get the QStyle::State_MouseOver
        QPoint cursorPos = QCursor::pos();
        QWidget *vp = m_ui.editorList->viewport();
        QMouseEvent e(QEvent::MouseMove, vp->mapFromGlobal(cursorPos), cursorPos, Qt::NoButton, 0, 0);
        QCoreApplication::sendEvent(vp, &e);
    }
}


NavigationView OpenEditorsViewFactory::createWidget()
{
    NavigationView n;
    n.widget = new OpenEditorsWidget();
    return n;
}

QString OpenEditorsViewFactory::displayName()
{
    return "Open Documents";
}

QKeySequence OpenEditorsViewFactory::activationSequence()
{
    return QKeySequence(Qt::ALT + Qt::Key_O);
}

OpenEditorsViewFactory::OpenEditorsViewFactory()
{
}

OpenEditorsViewFactory::~OpenEditorsViewFactory()
{
}
