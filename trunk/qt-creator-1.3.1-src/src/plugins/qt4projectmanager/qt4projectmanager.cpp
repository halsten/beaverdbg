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

#include "qt4projectmanager.h"

#include "qt4projectmanagerconstants.h"
#include "qt4projectmanagerplugin.h"
#include "qt4nodes.h"
#include "qt4project.h"
#include "profilereader.h"
#include "qmakestep.h"

#include <coreplugin/icore.h>
#include <coreplugin/basefilewizard.h>
#include <coreplugin/messagemanager.h>
#include <coreplugin/uniqueidmanager.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/iversioncontrol.h>
#include <coreplugin/vcsmanager.h>
#include <projectexplorer/buildmanager.h>
#include <projectexplorer/session.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <utils/listutils.h>
#include <designer/formwindoweditor.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QLinkedList>
#include <QtCore/QVariant>
#include <QtGui/QFileDialog>
#include <QtGui/QMenu>
#include <QtGui/QMessageBox>

using namespace Qt4ProjectManager;
using namespace Qt4ProjectManager::Internal;

using ProjectExplorer::BuildStep;
using ProjectExplorer::FileType;
using ProjectExplorer::HeaderType;
using ProjectExplorer::SourceType;
using ProjectExplorer::FormType;
using ProjectExplorer::ResourceType;
using ProjectExplorer::UnknownFileType;

// Known file types of a Qt 4 project
static const char* qt4FileTypes[] = {
    "CppHeaderFiles",
    "CppSourceFiles",
    "Qt4FormFiles",
    "Qt4ResourceFiles"
};

Qt4Manager::Qt4Manager(Qt4ProjectManagerPlugin *plugin)
  : m_mimeType(QLatin1String(Qt4ProjectManager::Constants::PROFILE_MIMETYPE)),
    m_plugin(plugin),
    m_projectExplorer(0),
    m_contextProject(0),
    m_languageID(0),
    m_lastEditor(0),
    m_dirty(false)
{
    m_languageID = Core::UniqueIDManager::instance()->
                   uniqueIdentifier(ProjectExplorer::Constants::LANG_CXX);
}

Qt4Manager::~Qt4Manager()
{
}

void Qt4Manager::registerProject(Qt4Project *project)
{
    m_projects.append(project);
}

void Qt4Manager::unregisterProject(Qt4Project *project)
{
    m_projects.removeOne(project);
}

void Qt4Manager::notifyChanged(const QString &name)
{
    foreach (Qt4Project *pro, m_projects)
        pro->notifyChanged(name);
}

void Qt4Manager::init()
{
    m_projectExplorer = ProjectExplorer::ProjectExplorerPlugin::instance();
    connect(Core::EditorManager::instance(), SIGNAL(editorAboutToClose(Core::IEditor*)),
            this, SLOT(editorAboutToClose(Core::IEditor*)));

    connect(Core::EditorManager::instance(), SIGNAL(currentEditorChanged(Core::IEditor*)),
            this, SLOT(editorChanged(Core::IEditor*)));
}

void Qt4Manager::editorChanged(Core::IEditor *editor)
{
    // Handle old editor
    Designer::FormWindowEditor *lastEditor = qobject_cast<Designer::FormWindowEditor *>(m_lastEditor);
    if (lastEditor) {
        disconnect(lastEditor, SIGNAL(changed()), this, SLOT(uiEditorContentsChanged()));

        if (m_dirty) {
            foreach(Qt4Project *project, m_projects)
                project->rootProjectNode()->updateCodeModelSupportFromEditor(lastEditor->file()->fileName(), lastEditor);
            m_dirty = false;
        }
    }

    m_lastEditor = editor;

    // Handle new editor
    if (Designer::FormWindowEditor *fw = qobject_cast<Designer::FormWindowEditor *>(editor))
        connect(fw, SIGNAL(changed()), this, SLOT(uiEditorContentsChanged()));
}

void Qt4Manager::editorAboutToClose(Core::IEditor *editor)
{
    if (m_lastEditor == editor) {
        // Oh no our editor is going to be closed
        // get the content first
        Designer::FormWindowEditor *lastEditor = qobject_cast<Designer::FormWindowEditor *>(m_lastEditor);
        if (lastEditor) {
            disconnect(lastEditor, SIGNAL(changed()), this, SLOT(uiEditorContentsChanged()));
            if (m_dirty) {
                foreach(Qt4Project *project, m_projects)
                    project->rootProjectNode()->updateCodeModelSupportFromEditor(lastEditor->file()->fileName(), lastEditor);
                m_dirty = false;
            }
        }
        m_lastEditor = 0;
    }
}

void Qt4Manager::uiEditorContentsChanged()
{
    // cast sender, get filename
    if (m_dirty)
        return;
    Designer::FormWindowEditor *fw = qobject_cast<Designer::FormWindowEditor *>(sender());
    if (!fw)
        return;
    m_dirty = true;
}

int Qt4Manager::projectContext() const
{
     return m_plugin->projectContext();
}

int Qt4Manager::projectLanguage() const
{
    return m_languageID;
}

QString Qt4Manager::mimeType() const
{
    return m_mimeType;
}

ProjectExplorer::Project* Qt4Manager::openProject(const QString &fileName)
{
    Core::MessageManager *messageManager = Core::ICore::instance()->messageManager();
    messageManager->displayStatusBarMessage(tr("Loading project %1 ...").arg(fileName), 50000);

    // TODO Make all file paths relative & remove this hack
    // We convert the path to an absolute one here because qt4project.cpp
    // && profileevaluator use absolute/canonical file paths all over the place
    // Correct fix would be to remove these calls ...
    QString canonicalFilePath = QFileInfo(fileName).canonicalFilePath();

    if (canonicalFilePath.isEmpty()) {
        messageManager->printToOutputPane(tr("Failed opening project '%1': Project file does not exist").arg(QDir::toNativeSeparators(canonicalFilePath)));
        messageManager->displayStatusBarMessage(tr("Failed opening project"), 5000);
        return 0;
    }

    foreach (ProjectExplorer::Project *pi, projectExplorer()->session()->projects()) {
        if (canonicalFilePath == pi->file()->fileName()) {
            messageManager->printToOutputPane(tr("Failed opening project '%1': Project already open").arg(QDir::toNativeSeparators(canonicalFilePath)));
            messageManager->displayStatusBarMessage(tr("Failed opening project"), 5000);
            return 0;
        }
    }

    messageManager->displayStatusBarMessage(tr("Opening %1 ...").arg(fileName));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    Qt4Project *pro = new Qt4Project(this, canonicalFilePath);

    messageManager->displayStatusBarMessage(tr("Done opening project"), 5000);
    return pro;
}

ProjectExplorer::ProjectExplorerPlugin *Qt4Manager::projectExplorer() const
{
    return m_projectExplorer;
}

ProjectExplorer::Node *Qt4Manager::contextNode() const
{
    return m_contextNode;
}

void Qt4Manager::setContextNode(ProjectExplorer::Node *node)
{
    m_contextNode = node;
}

void Qt4Manager::setContextProject(ProjectExplorer::Project *project)
{
    m_contextProject = project;
}

ProjectExplorer::Project *Qt4Manager::contextProject() const
{
    return m_contextProject;
}

void Qt4Manager::runQMake()
{
    runQMake(m_projectExplorer->currentProject());
}

void Qt4Manager::runQMakeContextMenu()
{
    runQMake(m_contextProject);
}

void Qt4Manager::runQMake(ProjectExplorer::Project *p)
{
    QMakeStep *qs = qobject_cast<Qt4Project *>(p)->qmakeStep();
    if (!qs)
        return;
    //found qmakeStep, now use it
    qs->setForced(true);
    m_projectExplorer->buildManager()->appendStep(qs, p->activeBuildConfiguration()->name());
}

QString Qt4Manager::fileTypeId(ProjectExplorer::FileType type)
{
    switch (type) {
    case HeaderType:
        return QLatin1String(qt4FileTypes[0]);
    case SourceType:
        return QLatin1String(qt4FileTypes[1]);
    case FormType:
        return QLatin1String(qt4FileTypes[2]);
    case ResourceType:
        return QLatin1String(qt4FileTypes[3]);
    case UnknownFileType:
    default:
        break;
    }
    return QString();
}
