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

#ifndef DISASSEMBLERHANDLER_H
#define DISASSEMBLERHANDLER_H

#include <QtCore/QObject>
#include <QtCore/QAbstractItemModel>

#include <QtGui/QIcon>

namespace Debugger {
namespace Internal {

class DisassemblerLine
{
public:
    void clear();

    QString address;
    QString symbol;
    QString addressDisplay;
    QString symbolDisplay;
    QString mnemonic;
};

class DisassemblerModel;

class DisassemblerHandler : public QObject
{
    Q_OBJECT

public:
    DisassemblerHandler();
    QAbstractItemModel *model() const;

public slots:
    void removeAll();

    void setLines(const QList<DisassemblerLine> &lines);
    QList<DisassemblerLine> lines() const;
    void setCurrentLine(int line);

private:
    DisassemblerModel *m_model;
};

} // namespace Internal
} // namespace Debugger

#endif // DISASSEMBLERHANDLER_H
