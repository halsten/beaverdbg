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

#ifndef DEBUGGER_REGISTERHANDLER_H
#define DEBUGGER_REGISTERHANDLER_H

#include <QtCore/QAbstractTableModel>

namespace Debugger {
namespace Internal {

enum RegisterRole
{
    RegisterNumberBaseRole = Qt::UserRole, // Currently used number base
    RegisterAddressRole,                   // Start value for opening memory view
    RegisterChangedRole                    // Used for painting changed values 
};

class Register
{
public:
    Register() : changed(true) {}
    Register(QString const &name_) : name(name_), changed(true) {}

public:
    QString name;
    QString value;
    bool changed;
};

class RegisterHandler : public QAbstractTableModel
{
    Q_OBJECT

public:
    RegisterHandler(QObject *parent = 0);

    QAbstractItemModel *model() { return this; }

    bool isEmpty() const; // nothing known so far?
    void setRegisters(const QList<Register> &registers);
    QList<Register> registers() const;
    void removeAll();
    Q_SLOT void setNumberBase(int base);

private:
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation,
        int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags(const QModelIndex &idx) const;
    
    QList<Register> m_registers;
    int m_base;
    int m_strlen; // approximate width of an value in chars
};

} // namespace Internal
} // namespace Debugger

#endif // DEBUGGER_REGISTERHANDLER_H
