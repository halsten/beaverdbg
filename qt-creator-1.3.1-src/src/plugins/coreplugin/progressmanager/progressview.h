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

#ifndef PROGRESSVIEW_H
#define PROGRESSVIEW_H

#include "progressmanager.h"

#include <QtCore/QFuture>
#include <QtGui/QWidget>
#include <QtGui/QIcon>
#include <QtGui/QVBoxLayout>

namespace Core {

class FutureProgress;

namespace Internal {

class ProgressView : public QWidget
{
    Q_OBJECT

public:
    ProgressView(QWidget *parent = 0);
    ~ProgressView();

    /** The returned FutureProgress instance is guaranteed to live till next main loop event processing (deleteLater). */
    FutureProgress *addTask(const QFuture<void> &future,
                            const QString &title,
                            const QString &type,
                            ProgressManager::PersistentType persistency);

private slots:
    void slotFinished();

private:
    void removeOldTasks(const QString &type, bool keepOne = false);
    void removeOneOldTask();
    void removeTask(FutureProgress *task);
    void deleteTask(FutureProgress *task);

    QVBoxLayout *m_layout;
    QList<FutureProgress *> m_taskList;
    QHash<FutureProgress *, QString> m_type;
    QHash<FutureProgress *, bool> m_keep;
};

} // namespace Internal
} // namespace Core

#endif // PROGRESSVIEW_H
