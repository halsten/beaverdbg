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

#ifndef LOCATORMANAGER_H
#define LOCATORMANAGER_H

#include "locator_global.h"

#include <QtCore/QObject>

namespace Locator {

namespace Internal {
class LocatorWidget;
}

class LOCATOR_EXPORT LocatorManager : public QObject
{
    Q_OBJECT

public:
    LocatorManager(Internal::LocatorWidget *locatorWidget);
    ~LocatorManager();

    static LocatorManager* instance() { return m_instance; }

    void show(const QString &text, int selectionStart = -1, int selectionLength = 0);

private:
    Internal::LocatorWidget *m_locatorWidget;
    static LocatorManager *m_instance;
};

} // namespace Locator

#endif // LOCATORMANAGER_H
