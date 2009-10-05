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

#include "registerhandler.h"

#include "debuggerconstants.h"

#include <utils/qtcassert.h>

#include <QtCore/QAbstractTableModel>
#include <QtCore/QDebug>

#include <QtGui/QColor>

using namespace Debugger;
using namespace Debugger::Internal;
using namespace Debugger::Constants;


//////////////////////////////////////////////////////////////////
//
// RegisterHandler
//
//////////////////////////////////////////////////////////////////

RegisterHandler::RegisterHandler(QObject *parent)
  : QAbstractTableModel(parent)
{}

int RegisterHandler::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_registers.size();
}

int RegisterHandler::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 2;
}

QVariant RegisterHandler::data(const QModelIndex &index, int role) const
{
    static const QVariant red = QColor(200, 0, 0);
    if (!index.isValid() || index.row() >= m_registers.size())
        return QVariant();

    const Register &reg = m_registers.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0: return reg.name;
            case 1: return reg.value;
        }
    }
    if (role == Qt::TextColorRole && reg.changed && index.column() == 1)
        return red;
    return QVariant();
}

QVariant RegisterHandler::headerData(int section, Qt::Orientation orientation,
    int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case 0: return tr("Name");
            case 1: return tr("Value");
        };
    }
    return QVariant();
}

void RegisterHandler::removeAll()
{
    m_registers.clear();
    reset();
}

bool RegisterHandler::isEmpty() const
{
    return m_registers.isEmpty();
}

void RegisterHandler::setRegisters(const QList<Register> &registers)
{
    m_registers = registers;
    reset();
}

QList<Register> RegisterHandler::registers() const
{
    return m_registers;
}
