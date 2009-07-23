/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact:  Qt Software Information (qt-info@nokia.com)
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
** contact the sales department at qt-sales@nokia.com.
**
**************************************************************************/

#include "messagemanager.h"
#include "messageoutputwindow.h"

#include <extensionsystem/pluginmanager.h>

#include <QtGui/QStatusBar>
#include <QtGui/QApplication>

using namespace Core;

MessageManager *MessageManager::m_instance = 0;

MessageManager::MessageManager()
    : m_messageOutputWindow(0)
{
    m_instance = this;
}

MessageManager::~MessageManager()
{
    ExtensionSystem::PluginManager *pm = ExtensionSystem::PluginManager::instance();
    if (pm && m_messageOutputWindow) {
        pm->removeObject(m_messageOutputWindow);
        delete m_messageOutputWindow;
    }

    m_instance = 0;
}

void MessageManager::init()
{
    m_messageOutputWindow = new Internal::MessageOutputWindow;
    ExtensionSystem::PluginManager::instance()->addObject(m_messageOutputWindow);
}

void MessageManager::showOutputPane()
{
    if (m_messageOutputWindow)
        m_messageOutputWindow->popup(false);
}

void MessageManager::displayStatusBarMessage(const QString & /*text*/, int /*ms*/)
{
    // TODO: Currently broken, but noone really notices, so...
    //m_mainWindow->statusBar()->showMessage(text, ms);
}

void MessageManager::printToOutputPane(const QString &text, bool bringToForeground)
{
    if (!m_messageOutputWindow)
        return;
    if (bringToForeground)
        m_messageOutputWindow->popup(false);
    m_messageOutputWindow->append(text);
}
