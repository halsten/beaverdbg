/**************************************************************************
**
** This file is part of Beaver Debugger
**
** Copyright (c) 2009 Andrei Kopats aka hlamer <hlamer@tut.by>
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
**************************************************************************/
#ifndef BEAVERVERSIONDIALOG_H
#define BEAVERVERSIONDIALOG_H

#include <QtGui/QDialog>

namespace Core {
namespace Internal {

class BeaverVersionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BeaverVersionDialog(QWidget *parent);
};

} // namespace Internal
} // namespace Core

#endif // BEAVERVERSIONDIALOG_H
