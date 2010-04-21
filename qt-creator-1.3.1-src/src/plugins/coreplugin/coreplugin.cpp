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

#include "coreplugin.h"
#include "editmode.h"
#include "editormanager.h"
#include "mainwindow.h"
#include "modemanager.h"
#include "fileiconprovider.h"

#include <extensionsystem/pluginmanager.h>

#include <QtCore/QtPlugin>

using namespace Core::Internal;

CorePlugin::CorePlugin() :
    m_mainWindow(new MainWindow), m_editMode(0)
{
	Q_INIT_RESOURCE(core);
	Q_INIT_RESOURCE(fancyactionbar);
}

CorePlugin::~CorePlugin()
{
    if (m_editMode) {
        removeObject(m_editMode);
        delete m_editMode;
    }

    // delete FileIconProvider singleton
    delete FileIconProvider::instance();

    delete m_mainWindow;
}

bool CorePlugin::initialize(const QStringList &arguments, QString *errorMessage)
{
    Q_UNUSED(arguments)
    const bool success = m_mainWindow->init(errorMessage);
    if (success) {
        EditorManager *editorManager = m_mainWindow->editorManager();
        m_editMode = new EditMode(editorManager);
        addObject(m_editMode);
    }
    return success;
}

void CorePlugin::extensionsInitialized()
{
    m_mainWindow->extensionsInitialized();
}

void CorePlugin::remoteArgument(const QString& arg)
{
    // An empty argument is sent to trigger activation
    // of the window via QtSingleApplication. It should be
    // the last of a sequence.
    if (arg.isEmpty()) {
        m_mainWindow->activateWindow();
    } else {
        m_mainWindow->openFiles(QStringList(arg));
    }
}

void CorePlugin::shutdown()
{
    m_mainWindow->shutdown();
}

//Q_EXPORT_PLUGIN(CorePlugin)
