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

#ifndef REMOVEFILEDIALOG_H
#define REMOVEFILEDIALOG_H

#include <QtGui/QDialog>

namespace ProjectExplorer {
namespace Internal {

namespace Ui {
    class RemoveFileDialog;
}

class RemoveFileDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(RemoveFileDialog)
public:
    explicit RemoveFileDialog(const QString &filePath, QWidget *parent = 0);
    virtual ~RemoveFileDialog();

    bool isDeleteFileChecked() const;

protected:
    virtual void changeEvent(QEvent *e);

private:
    Ui::RemoveFileDialog *m_ui;
};

} // namespace Internal
} // namespace ProjectExplorer

#endif // REMOVEFILEDIALOG_H
