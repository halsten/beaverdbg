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

#include "proeditormodel.h"

#include "profilereader.h"
#include "prowriter.h"
#include "qt4nodes.h"
#include "qt4project.h"
#include "qt4projectmanager.h"
#include "qtuicodemodelsupport.h"

#include <projectexplorer/nodesvisitor.h>
#include <projectexplorer/filewatcher.h>

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/fileiconprovider.h>
#include <coreplugin/filemanager.h>
#include <coreplugin/icore.h>
#include <coreplugin/iversioncontrol.h>
#include <coreplugin/vcsmanager.h>

#include <cpptools/cppmodelmanagerinterface.h>
#include <cplusplus/CppDocument.h>
#include <extensionsystem/pluginmanager.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/buildmanager.h>

#include <utils/qtcassert.h>

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>

#include <QtGui/QPainter>
#include <QtGui/QMainWindow>
#include <QtGui/QMessageBox>
#include <QtGui/QPushButton>

using namespace Qt4ProjectManager;
using namespace Qt4ProjectManager::Internal;

namespace {
    bool debug = false;
}

namespace {
    // sorting helper function
    bool sortProjectFilesByPath(ProFile *f1, ProFile *f2)
    {
        return f1->fileName() < f2->fileName();
    }
}

/*!
  \class Qt4PriFileNode
  Implements abstract ProjectNode class
  */

Qt4PriFileNode::Qt4PriFileNode(Qt4Project *project, Qt4ProFileNode* qt4ProFileNode, const QString &filePath)
        : ProjectNode(filePath),
          m_project(project),
          m_qt4ProFileNode(qt4ProFileNode),
          m_projectFilePath(QDir::fromNativeSeparators(filePath)),
          m_projectDir(QFileInfo(filePath).absolutePath()),
          m_fileWatcher(new ProjectExplorer::FileWatcher(this))
{
    Q_ASSERT(project);
    setFolderName(QFileInfo(filePath).completeBaseName());

    static QIcon dirIcon;
    if (dirIcon.isNull()) {
        // Create a custom Qt dir icon based on the system icon
        Core::FileIconProvider *iconProvider = Core::FileIconProvider::instance();
        QPixmap dirIconPixmap = iconProvider->overlayIcon(QStyle::SP_DirIcon,
                                                          QIcon(":/qt4projectmanager/images/qt_project.png"),
                                                          QSize(16, 16));
        dirIcon.addPixmap(dirIconPixmap);
    }
    setIcon(dirIcon);
    m_fileWatcher->addFile(filePath);
    connect(m_fileWatcher, SIGNAL(fileChanged(QString)),
            this, SLOT(scheduleUpdate()));
}

void Qt4PriFileNode::scheduleUpdate()
{
    m_qt4ProFileNode->scheduleUpdate();
}

namespace Qt4ProjectManager {
namespace Internal {
    struct InternalNode
    {
        QMap<QString, InternalNode*> subnodes;
        QStringList files;
        ProjectExplorer::FileType type;
        QString fullName;
        QIcon icon;

        InternalNode()
        {
            type = ProjectExplorer::UnknownFileType;
        }

        ~InternalNode()
        {
            qDeleteAll(subnodes);
        }

        // Creates a tree structure from a list of absolute file paths.
        // Empty directories are compressed into a single entry with a longer path.
        // * project
        //    * /absolute/path
        //       * file1
        //    * relative
        //       * path1
        //          * file1
        //          * file2
        //       * path2
        //          * file1
        void create(const QString &projectDir, const QStringList &newFilePaths, ProjectExplorer::FileType type)
        {
            static const QChar separator = QChar('/');
            const QString projectDirWithSeparator = projectDir + separator;
            int projectDirWithSeparatorLength = projectDirWithSeparator.length();
            foreach (const QString &file, newFilePaths) {
                QString fileWithoutPrefix;
                bool isRelative;
                if (file.startsWith(projectDirWithSeparator)) {
                    isRelative = true;
                    fileWithoutPrefix = file.mid(projectDirWithSeparatorLength);
                } else {
                    isRelative = false;
                    fileWithoutPrefix = file;
                }
                QStringList parts = fileWithoutPrefix.split(separator, QString::SkipEmptyParts);
                if (!isRelative && parts.count() > 0)
                    parts[0].prepend(separator);
                QStringListIterator it(parts);
                InternalNode *currentNode = this;
                QString path = (isRelative ? projectDir : "");
                while (it.hasNext()) {
                    const QString &key = it.next();
                    path += separator + key;
                    if (it.hasNext()) { // key is directory
                        if (!currentNode->subnodes.contains(key)) {
                            InternalNode *val = new InternalNode;
                            val->type = type;
                            val->fullName = path;
                            currentNode->subnodes.insert(key, val);
                            currentNode = val;
                        } else {
                            currentNode = currentNode->subnodes.value(key);
                        }
                    } else { // key is filename
                        currentNode->files.append(file);
                    }
                }
            }
            this->compress();
        }

        // Removes folder nodes with only a single sub folder in it
        void compress()
        {
            static const QChar separator = QChar('/');
            QMap<QString, InternalNode*> newSubnodes;
            QMapIterator<QString, InternalNode*> i(subnodes);
            while (i.hasNext()) {
                i.next();
                i.value()->compress();
                if (i.value()->files.isEmpty() && i.value()->subnodes.size() == 1) {
                    QString key = i.value()->subnodes.keys().at(0);
                    newSubnodes.insert(i.key()+separator+key, i.value()->subnodes.value(key));
                    i.value()->subnodes.clear();
                    delete i.value();
                } else {
                    newSubnodes.insert(i.key(), i.value());
                }
            }
            subnodes = newSubnodes;
        }

        // Makes the projectNode's subtree below the given folder match this internal node's subtree
        void updateSubFolders(Qt4PriFileNode *projectNode, ProjectExplorer::FolderNode *folder)
        {
            updateFiles(projectNode, folder, type);

            // update folders
            QList<FolderNode *> existingFolderNodes;
            foreach (FolderNode *node, folder->subFolderNodes()) {
                if (node->nodeType() != ProjectNodeType)
                    existingFolderNodes << node;
            }

            QList<FolderNode *> foldersToRemove;
            QList<FolderNode *> foldersToAdd;
            QList<InternalNode *> internalNodesToUpdate;
            QList<FolderNode *> folderNodesToUpdate;

            // newFolders is already sorted
            qSort(existingFolderNodes.begin(), existingFolderNodes.end(), ProjectNode::sortFolderNodesByName);

            QList<FolderNode*>::const_iterator existingNodeIter = existingFolderNodes.constBegin();
            QMap<QString, InternalNode*>::const_iterator newNodeIter = subnodes.constBegin();;
            while (existingNodeIter != existingFolderNodes.constEnd()
                   && newNodeIter != subnodes.constEnd()) {
                if ((*existingNodeIter)->name() < newNodeIter.key()) {
                    foldersToRemove << *existingNodeIter;
                    ++existingNodeIter;
                } else if ((*existingNodeIter)->name() > newNodeIter.key()) {
                    FolderNode *newNode = new FolderNode(newNodeIter.value()->fullName);
                    newNode->setFolderName(newNodeIter.key());
                    if (!newNodeIter.value()->icon.isNull())
                        newNode->setIcon(newNodeIter.value()->icon);
                    foldersToAdd << newNode;
                    internalNodesToUpdate << newNodeIter.value();
                    folderNodesToUpdate << newNode;
                    ++newNodeIter;
                } else { // *existingNodeIter->path() == *newPathIter
                    internalNodesToUpdate << newNodeIter.value();
                    folderNodesToUpdate << *existingNodeIter;
                    ++existingNodeIter;
                    ++newNodeIter;
                }
            }

            while (existingNodeIter != existingFolderNodes.constEnd()) {
                foldersToRemove << *existingNodeIter;
                ++existingNodeIter;
            }
            while (newNodeIter != subnodes.constEnd()) {
                FolderNode *newNode = new FolderNode(newNodeIter.value()->fullName);
                newNode->setFolderName(newNodeIter.key());
                if (!newNodeIter.value()->icon.isNull())
                    newNode->setIcon(newNodeIter.value()->icon);
                foldersToAdd << newNode;
                internalNodesToUpdate << newNodeIter.value();
                folderNodesToUpdate << newNode;
                ++newNodeIter;
            }

            if (!foldersToRemove.isEmpty())
                projectNode->removeFolderNodes(foldersToRemove, folder);
            if (!foldersToAdd.isEmpty())
                projectNode->addFolderNodes(foldersToAdd, folder);

            QList<FolderNode *>::const_iterator folderIt = folderNodesToUpdate.constBegin();
            QList<InternalNode *>::const_iterator iNodeIt = internalNodesToUpdate.constBegin();
            while (folderIt != folderNodesToUpdate.constEnd()
                    && iNodeIt != internalNodesToUpdate.constEnd()) {
                (*iNodeIt)->updateSubFolders(projectNode, *folderIt);
                ++folderIt;
                ++iNodeIt;
            }
        }

        // Makes the folder's files match this internal node's file list
        void updateFiles(Qt4PriFileNode *projectNode, FolderNode *folder, FileType type)
        {
            QList<FileNode*> existingFileNodes;
            foreach (FileNode *fileNode, folder->fileNodes()) {
                if (fileNode->fileType() == type && !fileNode->isGenerated())
                    existingFileNodes << fileNode;
            }

            QList<FileNode*> filesToRemove;
            QList<FileNode*> filesToAdd;

            qSort(files);
            qSort(existingFileNodes.begin(), existingFileNodes.end(), ProjectNode::sortNodesByPath);

            QList<FileNode*>::const_iterator existingNodeIter = existingFileNodes.constBegin();
            QList<QString>::const_iterator newPathIter = files.constBegin();
            while (existingNodeIter != existingFileNodes.constEnd()
                   && newPathIter != files.constEnd()) {
                if ((*existingNodeIter)->path() < *newPathIter) {
                    filesToRemove << *existingNodeIter;
                    ++existingNodeIter;
                } else if ((*existingNodeIter)->path() > *newPathIter) {
                    filesToAdd << new FileNode(*newPathIter, type, false);
                    ++newPathIter;
                } else { // *existingNodeIter->path() == *newPathIter
                    ++existingNodeIter;
                    ++newPathIter;
                }
            }
            while (existingNodeIter != existingFileNodes.constEnd()) {
                filesToRemove << *existingNodeIter;
                ++existingNodeIter;
            }
            while (newPathIter != files.constEnd()) {
                filesToAdd << new FileNode(*newPathIter, type, false);
                ++newPathIter;
            }

            if (!filesToRemove.isEmpty())
                projectNode->removeFileNodes(filesToRemove, folder);
            if (!filesToAdd.isEmpty())
                projectNode->addFileNodes(filesToAdd, folder);
        }
    };
} // Internal namespace
} // namespace

void Qt4PriFileNode::update(ProFile *includeFile, ProFileReader *reader)
{
    Q_ASSERT(includeFile);
    Q_ASSERT(reader);

    // add project file node
    if (m_fileNodes.isEmpty())
        addFileNodes(QList<FileNode*>() << new FileNode(m_projectFilePath, ProjectFileType, false), this);

    static QList<FileType> fileTypes =
               (QList<FileType>() << ProjectExplorer::HeaderType
                                  << ProjectExplorer::SourceType
                                  << ProjectExplorer::FormType
                                  << ProjectExplorer::ResourceType
                                  << ProjectExplorer::UnknownFileType);
    static QStringList fileTypeNames =
                QStringList() << tr("Headers")
                              << tr("Sources")
                              << tr("Forms")
                              << tr("Resources")
                              << tr("Other files");
    static QList<QIcon> fileTypeIcons;
    if (fileTypeIcons.isEmpty()) {
        QStringList iconPaths;
        iconPaths << ":/qt4projectmanager/images/headers.png"
                  << ":/qt4projectmanager/images/sources.png"
                  << ":/qt4projectmanager/images/forms.png"
                  << ":/qt4projectmanager/images/qt_qrc.png"
                  << ":/qt4projectmanager/images/unknown.png";
        foreach (const QString &iconPath, iconPaths) {
            QIcon dirIcon;
            Core::FileIconProvider *iconProvider = Core::FileIconProvider::instance();
            QPixmap dirIconPixmap = iconProvider->overlayIcon(QStyle::SP_DirIcon,
                                                              QIcon(iconPath),
                                                              QSize(16, 16));
            dirIcon.addPixmap(dirIconPixmap);
            fileTypeIcons.append(dirIcon);
        }
    }

    const QString &projectDir = m_qt4ProFileNode->m_projectDir;

    QStringList baseVPaths;
    baseVPaths += reader->absolutePathValues("VPATH", projectDir);
    baseVPaths << projectDir; // QMAKE_ABSOLUTE_SOURCE_PATH
    baseVPaths += reader->absolutePathValues("DEPENDPATH", projectDir);
    baseVPaths.removeDuplicates();

    InternalNode contents;

    // update files
    for (int i = 0; i < fileTypes.size(); ++i) {
        FileType type = fileTypes.at(i);
        const QStringList qmakeVariables = varNames(type);

        QStringList newFilePaths;
        foreach (const QString &qmakeVariable, qmakeVariables) {
            QStringList vPaths;
            if (type == ProjectExplorer::SourceType)
                vPaths = reader->absolutePathValues("VPATH_" + qmakeVariable, projectDir);
            vPaths += baseVPaths;
            if (type == ProjectExplorer::HeaderType)
                vPaths += reader->absolutePathValues("INCLUDEPATH", projectDir);
            vPaths.removeDuplicates();
            newFilePaths += reader->absoluteFileValues(qmakeVariable, projectDir, vPaths, includeFile);
        }
        newFilePaths.removeDuplicates();

        if (!newFilePaths.isEmpty()) {
            InternalNode *subfolder = new InternalNode;
            subfolder->type = type;
            subfolder->icon = fileTypeIcons.at(i);
            subfolder->fullName = m_projectDir + '#' + fileTypeNames.at(i);
            contents.subnodes.insert(fileTypeNames.at(i), subfolder);
            // create the hierarchy with subdirectories
            subfolder->create(m_projectDir, newFilePaths, type);
        }
    }
    contents.updateSubFolders(this, this);
}

QList<ProjectNode::ProjectAction> Qt4PriFileNode::supportedActions() const
{
    QList<ProjectAction> actions;

    const FolderNode *folderNode = this;
    const Qt4ProFileNode *proFileNode;
    while (!(proFileNode = qobject_cast<const Qt4ProFileNode*>(folderNode)))
        folderNode = folderNode->parentFolderNode();
    Q_ASSERT(proFileNode);

    switch (proFileNode->projectType()) {
    case ApplicationTemplate:
    case LibraryTemplate:
        actions << AddFile << RemoveFile;
        break;
    case SubDirsTemplate:
        actions << AddSubProject << RemoveSubProject;
        break;
    default:
        break;
    }
    return actions;
}

bool Qt4PriFileNode::addSubProjects(const QStringList &proFilePaths)
{
    Q_UNUSED(proFilePaths)
    return false; //changeIncludes(m_includeFile, proFilePaths, AddToProFile);
}

bool Qt4PriFileNode::removeSubProjects(const QStringList &proFilePaths)
{
    Q_UNUSED(proFilePaths)
    return false; //changeIncludes(m_includeFile, proFilePaths, RemoveFromProFile);
}

bool Qt4PriFileNode::addFiles(const FileType fileType, const QStringList &filePaths,
                           QStringList *notAdded)
{
    QStringList failedFiles;

    changeFiles(fileType, filePaths, &failedFiles, AddToProFile);
    if (notAdded)
        *notAdded = failedFiles;
    return failedFiles.isEmpty();
}

bool Qt4PriFileNode::removeFiles(const FileType fileType, const QStringList &filePaths,
                              QStringList *notRemoved)
{
    QStringList failedFiles;
    changeFiles(fileType, filePaths, &failedFiles, RemoveFromProFile);
    if (notRemoved)
        *notRemoved = failedFiles;
    return failedFiles.isEmpty();
}

bool Qt4PriFileNode::renameFile(const FileType fileType, const QString &filePath,
                             const QString &newFilePath)
{
    if (newFilePath.isEmpty())
        return false;

    if (!QFile::rename(filePath, newFilePath))
        return false;

    QStringList dummy;
    changeFiles(fileType, QStringList() << filePath, &dummy, RemoveFromProFile);
    if (!dummy.isEmpty())
        return false;
    changeFiles(fileType, QStringList() << newFilePath, &dummy, AddToProFile);
    if (!dummy.isEmpty())
        return false;
    return true;
}

bool Qt4PriFileNode::changeIncludes(ProFile *includeFile, const QStringList &proFilePaths,
                                    ChangeType change)
{
    Q_UNUSED(includeFile)
    Q_UNUSED(proFilePaths)
    Q_UNUSED(change)
    // TODO
    return false;
}

bool Qt4PriFileNode::priFileWritable(const QString &path)
{
    const QString dir = QFileInfo(path).dir().path();
    Core::ICore *core = Core::ICore::instance();
    Core::IVersionControl *versionControl = core->vcsManager()->findVersionControlForDirectory(dir);
    switch (Core::EditorManager::promptReadOnlyFile(path, versionControl, core->mainWindow(), false)) {
    case Core::EditorManager::RO_OpenVCS:
        if (!versionControl->vcsOpen(path)) {
            QMessageBox::warning(core->mainWindow(), tr("Failed!"), tr("Could not open the file for edit with SCC."));
            return false;
        }
        break;
    case Core::EditorManager::RO_MakeWriteable: {
        const bool permsOk = QFile::setPermissions(path, QFile::permissions(path) | QFile::WriteUser);
        if (!permsOk) {
            QMessageBox::warning(core->mainWindow(), tr("Failed!"),  tr("Could not set permissions to writable."));
            return false;
        }
        break;
    }
    case Core::EditorManager::RO_SaveAs:
    case Core::EditorManager::RO_Cancel:
        return false;
    }
    return true;
}

bool Qt4PriFileNode::saveModifiedEditors(const QString &path)
{
    QList<Core::IFile*> allFileHandles;
    QList<Core::IFile*> modifiedFileHandles;

    Core::ICore *core = Core::ICore::instance();

    foreach (Core::IFile *file, core->fileManager()->managedFiles(path)) {
        allFileHandles << file;
    }

    foreach (Core::IEditor *editor, core->editorManager()->editorsForFileName(path)) {
        if (Core::IFile *editorFile = editor->file()) {
            if (editorFile->isModified())
                modifiedFileHandles << editorFile;
        }
    }

    if (!modifiedFileHandles.isEmpty()) {
        bool cancelled;
        core->fileManager()->saveModifiedFiles(modifiedFileHandles, &cancelled,
                                         tr("There are unsaved changes for project file %1.").arg(path));
        if (cancelled)
            return false;
        // force instant reload
        foreach (Core::IFile *fileHandle, allFileHandles) {
            Core::IFile::ReloadBehavior reload = Core::IFile::ReloadAll;
            fileHandle->modified(&reload);
        }
    }
    return true;
}

void Qt4PriFileNode::changeFiles(const FileType fileType,
                                 const QStringList &filePaths,
                                 QStringList *notChanged,
                                 ChangeType change)
{
    if (filePaths.isEmpty())
        return;

    ProFileReader *reader = m_qt4ProFileNode->createProFileReader();
    if (!reader->readProFile(m_qt4ProFileNode->path())) {
        m_project->proFileParseError(tr("Error while parsing file %1. Giving up.").arg(m_projectFilePath));
        delete reader;
        return;
    }

    ProFile *includeFile = reader->proFileFor(m_projectFilePath);
    if (!includeFile) {
        m_project->proFileParseError(tr("Error while changing pro file %1.").arg(m_projectFilePath));
    }

    *notChanged = filePaths;

    // Check for modified editors
    if (!saveModifiedEditors(m_projectFilePath)) {
        delete reader;
        return;
    }

    // Check if file is readonly
    ProEditorModel proModel;
    proModel.setProFiles(QList<ProFile*>() << includeFile);

    const QStringList vars = varNames(fileType);
    QDir priFileDir = QDir(m_projectDir);

    if (change == AddToProFile) {
        // root item "<Global Scope>"
        const QModelIndex root = proModel.index(0, 0);

        // Check if variable item exists as child of root item
        ProVariable *proVar = 0;
        int row = 0;
        for (; row < proModel.rowCount(root); ++row) {
            if ((proVar = proModel.proVariable(root.child(row, 0)))) {
                if (vars.contains(proVar->variable())
                    && proVar->variableOperator() != ProVariable::RemoveOperator
                    && proVar->variableOperator() != ProVariable::ReplaceOperator)
                    break;
                else
                    proVar = 0;
            }
        }

        if (!proVar) {
            // Create & append new variable item

            // TODO: This will always store e.g. a source file in SOURCES and not OBJECTIVE_SOURCES
            proVar = new ProVariable(vars.first(), proModel.proBlock(root));
            proVar->setVariableOperator(ProVariable::AddOperator);
            proModel.insertItem(proVar, row, root);
        }
        const QModelIndex varIndex = root.child(row, 0);

        const QString &proFilePath = includeFile->fileName();
        foreach (const QString &filePath, filePaths) {
            if (filePath == proFilePath)
                continue;
            const QString &relativeFilePath = priFileDir.relativeFilePath(filePath);
            proModel.insertItem(new ProValue(relativeFilePath, proVar),
                                proModel.rowCount(varIndex), varIndex);
            notChanged->removeOne(filePath);
        }
    } else { // RemoveFromProFile
        QList<QModelIndex> proVarIndexes = proModel.findVariables(vars);
        QList<QModelIndex> toRemove;

        QStringList relativeFilePaths;
        foreach (const QString &absoluteFilePath, filePaths)
            relativeFilePaths << priFileDir.relativeFilePath(absoluteFilePath);

        foreach (const QModelIndex &proVarIndex, proVarIndexes) {
            ProVariable *proVar = proModel.proVariable(proVarIndex);

            if (proVar->variableOperator() != ProVariable::RemoveOperator
                && proVar->variableOperator() != ProVariable::ReplaceOperator) {

                for (int row = proModel.rowCount(proVarIndex) - 1; row >= 0; --row) {
                    QModelIndex itemIndex = proModel.index(row, 0, proVarIndex);
                    ProItem *item = proModel.proItem(itemIndex);

                    if (item->kind() == ProItem::ValueKind) {
                        ProValue *val = static_cast<ProValue *>(item);
                        int index = relativeFilePaths.indexOf(val->value());
                        if (index != -1) {
                            toRemove.append(itemIndex);
                            notChanged->removeAt(index);
                        }
                    }
                }
            }
        }

        foreach (const QModelIndex &index, toRemove) {
            proModel.removeItem(index);
        }
    }

    // save file
    save(includeFile);
    delete reader;
}

void Qt4PriFileNode::save(ProFile *includeFile)
{
    Core::ICore *core = Core::ICore::instance();
    Core::FileManager *fileManager = core->fileManager();
    QList<Core::IFile *> allFileHandles = fileManager->managedFiles(includeFile->fileName());
    Core::IFile *modifiedFileHandle = 0;
    foreach (Core::IFile *file, allFileHandles)
        if (file->fileName() == includeFile->fileName())
            modifiedFileHandle = file;

    if (modifiedFileHandle)
        fileManager->blockFileChange(modifiedFileHandle);
    ProWriter pw;
    const bool ok = pw.write(includeFile, includeFile->fileName());
    Q_UNUSED(ok)
    includeFile->setModified(false);
    m_project->qt4ProjectManager()->notifyChanged(includeFile->fileName());
    if (modifiedFileHandle)
        fileManager->unblockFileChange(modifiedFileHandle);

    Core::IFile::ReloadBehavior tempBehavior =
            Core::IFile::ReloadAll;
    foreach (Core::IFile *file, allFileHandles)
        file->modified(&tempBehavior);
}

/*
  Deletes all subprojects/files/virtual folders
  */
void Qt4PriFileNode::clear()
{
    // delete files && folders && projects
    if (!fileNodes().isEmpty())
        removeFileNodes(fileNodes(), this);
    if (!subProjectNodes().isEmpty())
        removeProjectNodes(subProjectNodes());
    if (!subFolderNodes().isEmpty())
        removeFolderNodes(subFolderNodes(), this);
}

QStringList Qt4PriFileNode::varNames(FileType type)
{
    QStringList vars;
    switch (type) {
    case ProjectExplorer::HeaderType:
        vars << QLatin1String("HEADERS");
        break;
    case ProjectExplorer::SourceType:
        vars << QLatin1String("SOURCES");
        vars << QLatin1String("OBJECTIVE_SOURCES");
        vars << QLatin1String("LEXSOURCES");
        vars << QLatin1String("YACCSOURCES");
        break;
    case ProjectExplorer::ResourceType:
        vars << QLatin1String("RESOURCES");
        break;
    case ProjectExplorer::FormType:
        vars << QLatin1String("FORMS");
        break;
    default:
        vars << QLatin1String("OTHER_FILES");
        break;
    }
    return vars;
}

Qt4PriFileNode *Qt4PriFileNode::findProFileFor(const QString &fileName)
{
    if (fileName == path())
        return this;
    foreach (ProjectNode *pn, subProjectNodes())
        if (Qt4PriFileNode *qt4PriFileNode = qobject_cast<Qt4PriFileNode *>(pn))
            if (Qt4PriFileNode *result = qt4PriFileNode->findProFileFor(fileName))
                return result;
    return 0;
}

/*!
  \class Qt4ProFileNode
  Implements abstract ProjectNode class
  */
Qt4ProFileNode::Qt4ProFileNode(Qt4Project *project,
                               const QString &filePath,
                               QObject *parent)
        : Qt4PriFileNode(project, this, filePath),
          // own stuff
          m_projectType(InvalidProject)
{
    if (parent)
        setParent(parent);

    m_updateTimer.setInterval(100);
    m_updateTimer.setSingleShot(true);

    connect(m_project, SIGNAL(activeBuildConfigurationChanged()),
            this, SLOT(update()));
    connect(&m_updateTimer, SIGNAL(timeout()),
            this, SLOT(update()));

    connect(ProjectExplorer::ProjectExplorerPlugin::instance()->buildManager(), SIGNAL(buildStateChanged(ProjectExplorer::Project*)),
            this, SLOT(buildStateChanged(ProjectExplorer::Project*)));
}

Qt4ProFileNode::~Qt4ProFileNode()
{
    CppTools::CppModelManagerInterface *modelManager
            = ExtensionSystem::PluginManager::instance()->getObject<CppTools::CppModelManagerInterface>();
    QMap<QString, Qt4UiCodeModelSupport *>::const_iterator it, end;
    end = m_uiCodeModelSupport.constEnd();
    for (it = m_uiCodeModelSupport.constBegin(); it != end; ++it) {
        modelManager->removeEditorSupport(it.value());
        delete it.value();
    }
}

void Qt4ProFileNode::buildStateChanged(ProjectExplorer::Project *project)
{
    if (project == m_project && !ProjectExplorer::ProjectExplorerPlugin::instance()->buildManager()->isBuilding(m_project)) {
        QStringList filesToUpdate = updateUiFiles();
        updateCodeModelSupportFromBuild(filesToUpdate);
    }
}

bool Qt4ProFileNode::hasTargets() const
{
    return (projectType() == ApplicationTemplate) || (projectType() == LibraryTemplate);
}

Qt4ProjectType Qt4ProFileNode::projectType() const
{
    return m_projectType;
}

QStringList Qt4ProFileNode::variableValue(const Qt4Variable var) const
{
    return m_varValues.value(var);
}

void Qt4ProFileNode::scheduleUpdate()
{
    m_updateTimer.start();
}

void Qt4ProFileNode::update()
{
    ProFileReader *reader = createProFileReader();
    if (!reader->readProFile(m_projectFilePath)) {
        m_project->proFileParseError(tr("Error while parsing file %1. Giving up.").arg(m_projectFilePath));
        delete reader;
        invalidate();
        return;
    }

    if (debug)
        qDebug() << "Qt4ProFileNode - updating files for file " << m_projectFilePath;

    Qt4ProjectType projectType = InvalidProject;
    switch (reader->templateType()) {
    case ProFileEvaluator::TT_Unknown:
    case ProFileEvaluator::TT_Application: {
        projectType = ApplicationTemplate;
        break;
    }
    case ProFileEvaluator::TT_Library: {
        projectType = LibraryTemplate;
        break;
    }
    case ProFileEvaluator::TT_Script: {
        projectType = ScriptTemplate;
        break;
    }
    case ProFileEvaluator::TT_Subdirs:
        projectType = SubDirsTemplate;
        break;
    }
    if (projectType != m_projectType) {
        Qt4ProjectType oldType = m_projectType;
        // probably all subfiles/projects have changed anyway ...
        clear();
        m_projectType = projectType;
        foreach (NodesWatcher *watcher, watchers())
            if (Qt4NodesWatcher *qt4Watcher = qobject_cast<Qt4NodesWatcher*>(watcher))
                emit qt4Watcher->projectTypeChanged(this, oldType, projectType);
    }

    //
    // Add/Remove pri files, sub projects
    //

    QList<ProjectNode*> existingProjectNodes = subProjectNodes();

    QList<QString> newProjectFiles;
    QHash<QString, ProFile*> includeFiles;
    ProFile *fileForCurrentProject = 0;
    {
        if (projectType == SubDirsTemplate) {
            foreach (const QString &subDirProject, subDirsPaths(reader))
                newProjectFiles << subDirProject;
        }

        foreach (ProFile *includeFile, reader->includeFiles()) {
            if (includeFile->fileName() == m_projectFilePath) { // this file
                fileForCurrentProject = includeFile;
            } else {
                newProjectFiles << includeFile->fileName();
                includeFiles.insert(includeFile->fileName(), includeFile);
            }
        }
    }

    qSort(existingProjectNodes.begin(), existingProjectNodes.end(),
          sortNodesByPath);
    qSort(newProjectFiles.begin(), newProjectFiles.end());

    QList<ProjectNode*> toAdd;
    QList<ProjectNode*> toRemove;

    QList<ProjectNode*>::const_iterator existingNodeIter = existingProjectNodes.constBegin();
    QList<QString>::const_iterator newProjectFileIter = newProjectFiles.constBegin();
    while (existingNodeIter != existingProjectNodes.constEnd()
               && newProjectFileIter != newProjectFiles.constEnd()) {
        if ((*existingNodeIter)->path() < *newProjectFileIter) {
            toRemove << *existingNodeIter;
            ++existingNodeIter;
        } else if ((*existingNodeIter)->path() > *newProjectFileIter) {
            if (ProFile *file = includeFiles.value(*newProjectFileIter)) {
                Qt4PriFileNode *priFileNode
                    = new Qt4PriFileNode(m_project, this,
                                         *newProjectFileIter);
                priFileNode->update(file, reader);
                toAdd << priFileNode;
            } else {
                toAdd << createSubProFileNode(*newProjectFileIter);
            }
            ++newProjectFileIter;
        } else { // *existingNodeIter->path() == *newProjectFileIter
             if (ProFile *file = includeFiles.value(*newProjectFileIter)) {
                Qt4PriFileNode *priFileNode = static_cast<Qt4PriFileNode*>(*existingNodeIter);
                priFileNode->update(file, reader);
            }

            ++existingNodeIter;
            ++newProjectFileIter;
        }
    }
    while (existingNodeIter != existingProjectNodes.constEnd()) {
        toRemove << *existingNodeIter;
        ++existingNodeIter;
    }
    while (newProjectFileIter != newProjectFiles.constEnd()) {
        if (ProFile *file = includeFiles.value(*newProjectFileIter)) {
            Qt4PriFileNode *priFileNode
                    = new Qt4PriFileNode(m_project, this,
                                         *newProjectFileIter);
            priFileNode->update(file, reader);
            toAdd << priFileNode;
        } else {
            toAdd << createSubProFileNode(*newProjectFileIter);
        }
        ++newProjectFileIter;
    }

    if (!toRemove.isEmpty())
        removeProjectNodes(toRemove);
    if (!toAdd.isEmpty())
        addProjectNodes(toAdd);

    Qt4PriFileNode::update(fileForCurrentProject, reader);

    // update other variables
    QHash<Qt4Variable, QStringList> newVarValues;

    newVarValues[DefinesVar] = reader->values(QLatin1String("DEFINES"));
    newVarValues[IncludePathVar] = includePaths(reader);
    newVarValues[UiDirVar] = uiDirPaths(reader);
    newVarValues[MocDirVar] = mocDirPaths(reader);
    newVarValues[PkgConfigVar] = reader->values(QLatin1String("PKGCONFIG"));

    if (m_varValues != newVarValues) {
        m_varValues = newVarValues;
        foreach (NodesWatcher *watcher, watchers())
            if (Qt4NodesWatcher *qt4Watcher = qobject_cast<Qt4NodesWatcher*>(watcher))
                emit qt4Watcher->variablesChanged(this, m_varValues, newVarValues);
    }

    createUiCodeModelSupport();
    updateUiFiles();

    foreach (NodesWatcher *watcher, watchers())
        if (Qt4NodesWatcher *qt4Watcher = qobject_cast<Qt4NodesWatcher*>(watcher))
            emit qt4Watcher->proFileUpdated(this);

    delete reader;
}

namespace {
    // find all ui files in project
    class FindUiFileNodesVisitor : public ProjectExplorer::NodesVisitor {
    public:
        void visitProjectNode(ProjectNode *projectNode)
        {
            visitFolderNode(projectNode);
        }
        void visitFolderNode(FolderNode *folderNode)
        {
            foreach (FileNode *fileNode, folderNode->fileNodes()) {
                if (fileNode->fileType() == ProjectExplorer::FormType)
                    uiFileNodes << fileNode;
            }
        }
        QList<FileNode*> uiFileNodes;
    };
}

// This function is triggered after a build, and updates the state ui files
// It does so by storing a modification time for each ui file we know about.

// TODO this function should also be called if the build directory is changed
QStringList Qt4ProFileNode::updateUiFiles()
{
//    qDebug()<<"Qt4ProFileNode::updateUiFiles()";
    // Only those two project types can have ui files for us
    if (m_projectType != ApplicationTemplate
        && m_projectType != LibraryTemplate)
        return QStringList();

    // Find all ui files
    FindUiFileNodesVisitor uiFilesVisitor;
    this->accept(&uiFilesVisitor);
    const QList<FileNode*> uiFiles = uiFilesVisitor.uiFileNodes;

    // Find the UiDir, there can only ever be one
    QString uiDir = buildDir();
    QStringList tmp = m_varValues[UiDirVar];
    if (tmp.size() != 0)
        uiDir = tmp.first();

    // Collect all existing generated files
    QList<FileNode*> existingFileNodes;
    foreach (FileNode *file, fileNodes()) {
        if (file->isGenerated())
            existingFileNodes << file;
    }

    // Convert uiFile to uiHeaderFilePath, find all headers that correspond
    // and try to find them in uiDir
    QStringList newFilePaths;
    foreach (FileNode *uiFile, uiFiles) {
        const QString uiHeaderFilePath
                = QString("%1/ui_%2.h").arg(uiDir, QFileInfo(uiFile->path()).completeBaseName());
        if (QFileInfo(uiHeaderFilePath).exists())
            newFilePaths << uiHeaderFilePath;
    }

    // Create a diff between those lists
    QList<FileNode*> toRemove;
    QList<FileNode*> toAdd;
    // The list of files for which we call updateSourceFile
    QStringList toUpdate;

    qSort(newFilePaths);
    qSort(existingFileNodes.begin(), existingFileNodes.end(), ProjectNode::sortNodesByPath);

    QList<FileNode*>::const_iterator existingNodeIter = existingFileNodes.constBegin();
    QList<QString>::const_iterator newPathIter = newFilePaths.constBegin();
    while (existingNodeIter != existingFileNodes.constEnd()
           && newPathIter != newFilePaths.constEnd()) {
        if ((*existingNodeIter)->path() < *newPathIter) {
            toRemove << *existingNodeIter;
            ++existingNodeIter;
        } else if ((*existingNodeIter)->path() > *newPathIter) {
            toAdd << new FileNode(*newPathIter, ProjectExplorer::HeaderType, true);
            ++newPathIter;
        } else { // *existingNodeIter->path() == *newPathIter
            QString fileName = (*existingNodeIter)->path();
            QMap<QString, QDateTime>::const_iterator it = m_uitimestamps.find(fileName);
            QDateTime lastModified = QFileInfo(fileName).lastModified();
            if (it == m_uitimestamps.constEnd() || it.value() < lastModified) {
                toUpdate << fileName;
                m_uitimestamps[fileName] = lastModified;
            }
            ++existingNodeIter;
            ++newPathIter;
        }
    }
    while (existingNodeIter != existingFileNodes.constEnd()) {
        toRemove << *existingNodeIter;
        ++existingNodeIter;
    }
    while (newPathIter != newFilePaths.constEnd()) {
        toAdd << new FileNode(*newPathIter, ProjectExplorer::HeaderType, true);
        ++newPathIter;
    }

    // Update project tree
    if (!toRemove.isEmpty()) {
        foreach (FileNode *file, toRemove)
            m_uitimestamps.remove(file->path());
        removeFileNodes(toRemove, this);
    }

    CppTools::CppModelManagerInterface *modelManager =
        ExtensionSystem::PluginManager::instance()->getObject<CppTools::CppModelManagerInterface>();

    if (!toAdd.isEmpty()) {
        foreach (FileNode *file, toAdd) {
            m_uitimestamps.insert(file->path(), QFileInfo(file->path()).lastModified());
            toUpdate << file->path();

            // Also adding files depending on that
            // We only need to do that for files that were newly created
            QString fileName = QFileInfo(file->path()).fileName();
            foreach (CPlusPlus::Document::Ptr doc, modelManager->snapshot()) {
                if (doc->includedFiles().contains(fileName)) {
                    if (!toUpdate.contains(doc->fileName()))
                        toUpdate << doc->fileName();
                }
            }
        }
        addFileNodes(toAdd, this);
    }
    return toUpdate;
}

ProFileReader *Qt4PriFileNode::createProFileReader() const
{
    ProFileReader *reader = new ProFileReader();
    connect(reader, SIGNAL(errorFound(QString)),
            m_project, SLOT(proFileParseError(QString)));

    QtVersion *version = m_project->qtVersion(m_project->activeBuildConfiguration());
    if (version->isValid())
        reader->setQtVersion(version);

    reader->setOutputDir(m_qt4ProFileNode->buildDir());

    return reader;
}

Qt4ProFileNode *Qt4ProFileNode::createSubProFileNode(const QString &path)
{
    Qt4ProFileNode *subProFileNode = new Qt4ProFileNode(m_project, path);
    subProFileNode->update();
    return subProFileNode;
}

QStringList Qt4ProFileNode::uiDirPaths(ProFileReader *reader) const
{
    QStringList candidates = reader->absolutePathValues(QLatin1String("UI_DIR"),
                                                        buildDir());
    candidates.removeDuplicates();
    return candidates;
}

QStringList Qt4ProFileNode::mocDirPaths(ProFileReader *reader) const
{
    QStringList candidates = reader->absolutePathValues(QLatin1String("MOC_DIR"),
                                                        buildDir());
    candidates.removeDuplicates();
    return candidates;
}

QStringList Qt4ProFileNode::includePaths(ProFileReader *reader) const
{
    QStringList paths;
    paths = reader->absolutePathValues(QLatin1String("INCLUDEPATH"),
                                       m_projectDir);
    paths << uiDirPaths(reader) << mocDirPaths(reader);
    paths.removeDuplicates();
    return paths;
}

QStringList Qt4ProFileNode::subDirsPaths(ProFileReader *reader) const
{
    QStringList subProjectPaths;

    const QStringList subDirVars = reader->values(QLatin1String("SUBDIRS"));

    foreach (const QString &subDirVar, subDirVars) {
        // Special case were subdir is just an identifier:
        //   "SUBDIR = subid
        //    subid.subdir = realdir"
        // or
        //   "SUBDIR = subid
        //    subid.file = realdir/realfile.pro"

        QString realDir;
        const QString subDirKey = subDirVar + QLatin1String(".subdir");
        const QString subDirFileKey = subDirVar + QLatin1String(".file");
        if (reader->contains(subDirKey))
            realDir = QFileInfo(reader->value(subDirKey)).filePath();
        else if (reader->contains(subDirFileKey))
            realDir = QFileInfo(reader->value(subDirFileKey)).filePath();
        else
            realDir = subDirVar;
        QFileInfo info(realDir);
        if (!info.isAbsolute()) {
            info.setFile(m_projectDir + "/" + realDir);
            realDir = m_projectDir + "/" + realDir;
        }

        QString realFile;
        if (info.isDir()) {
            realFile = QString("%1/%2.pro").arg(realDir, info.fileName());
            if (!QFile::exists(realFile)) {
                // parse directory for pro files - if there is only one, use that
                QDir dir(realDir);
                QStringList files = dir.entryList(QStringList() << "*.pro", QDir::Files);
                if (files.size() == 1) {
                    realFile = QString("%1/%2").arg(realDir, files.first());
                } else {
                    m_project->proFileParseError(tr("Could not find .pro file for sub dir '%1' in '%2'")
                            .arg(subDirVar).arg(realDir));
                    realFile = QString::null;
                }
            }
        } else {
            realFile = realDir;
        }

        if (!realFile.isEmpty() && !subProjectPaths.contains(realFile))
            subProjectPaths << realFile;
    }

    return subProjectPaths;
}

QString Qt4PriFileNode::buildDir() const
{
    const QDir srcDirRoot = QFileInfo(m_project->rootProjectNode()->path()).absoluteDir();
    const QString relativeDir = srcDirRoot.relativeFilePath(m_projectDir);
    return QDir(m_project->buildDirectory(m_project->activeBuildConfiguration())).absoluteFilePath(relativeDir);
}

/*
  Sets project type to InvalidProject & deletes all subprojects/files/virtual folders
  */
void Qt4ProFileNode::invalidate()
{
    if (m_projectType == InvalidProject)
        return;

    clear();

    // change project type
    Qt4ProjectType oldType = m_projectType;
    m_projectType = InvalidProject;


    foreach (NodesWatcher *watcher, watchers())
        if (Qt4NodesWatcher *qt4Watcher = qobject_cast<Qt4NodesWatcher*>(watcher))
            emit qt4Watcher->projectTypeChanged(this, oldType, InvalidProject);
}

void Qt4ProFileNode::updateCodeModelSupportFromBuild(const QStringList &files)
{
    foreach (const QString &file, files) {
        QMap<QString, Qt4UiCodeModelSupport *>::const_iterator it, end;
        end = m_uiCodeModelSupport.constEnd();
        for (it = m_uiCodeModelSupport.constBegin(); it != end; ++it) {
            if (it.value()->fileName() == file)
                it.value()->updateFromBuild();
        }
    }
}

void Qt4ProFileNode::updateCodeModelSupportFromEditor(const QString &uiFileName, Designer::FormWindowEditor *fw)
{
    QMap<QString, Qt4UiCodeModelSupport *>::const_iterator it;
    it = m_uiCodeModelSupport.constFind(uiFileName);
    if (it != m_uiCodeModelSupport.constEnd()) {
        it.value()->updateFromEditor(fw);
    }
    foreach (ProjectExplorer::ProjectNode *pro, subProjectNodes())
        if (Qt4ProFileNode *qt4proFileNode = qobject_cast<Qt4ProFileNode *>(pro))
            qt4proFileNode->updateCodeModelSupportFromEditor(uiFileName, fw);
}

void Qt4ProFileNode::createUiCodeModelSupport()
{
//    qDebug()<<"creatUiCodeModelSupport()";
    CppTools::CppModelManagerInterface *modelManager
            = ExtensionSystem::PluginManager::instance()->getObject<CppTools::CppModelManagerInterface>();

    // First move all to
    QMap<QString, Qt4UiCodeModelSupport *> oldCodeModelSupport;
    oldCodeModelSupport = m_uiCodeModelSupport;
    m_uiCodeModelSupport.clear();

    // Only those two project types can have ui files for us
    if (m_projectType == ApplicationTemplate || m_projectType == LibraryTemplate) {
        // Find all ui files
        FindUiFileNodesVisitor uiFilesVisitor;
        this->accept(&uiFilesVisitor);
        const QList<FileNode*> uiFiles = uiFilesVisitor.uiFileNodes;

        // Find the UiDir, there can only ever be one
        QString uiDir = buildDir();
        QStringList tmp = m_varValues[UiDirVar];
        if (tmp.size() != 0)
            uiDir = tmp.first();

        foreach (FileNode *uiFile, uiFiles) {
            QString uiHeaderFilePath
                    = QString("%1/ui_%2.h").arg(uiDir, QFileInfo(uiFile->path()).completeBaseName());
            uiHeaderFilePath = QDir::cleanPath(uiHeaderFilePath);

//            qDebug()<<"code model support for "<<uiFile->path()<<" "<<uiHeaderFilePath;
            QMap<QString, Qt4UiCodeModelSupport *>::iterator it = oldCodeModelSupport.find(uiFile->path());
            if (it != oldCodeModelSupport.end()) {
//                qDebug()<<"updated old codemodelsupport";
                Qt4UiCodeModelSupport *cms = it.value();
                cms->setFileName(uiHeaderFilePath);
                m_uiCodeModelSupport.insert(it.key(), cms);
                oldCodeModelSupport.erase(it);
            } else {
//                qDebug()<<"adding new codemodelsupport";
                Qt4UiCodeModelSupport *cms = new Qt4UiCodeModelSupport(modelManager, m_project, uiFile->path(), uiHeaderFilePath);
                m_uiCodeModelSupport.insert(uiFile->path(), cms);
                modelManager->addEditorSupport(cms);
            }
        }
    }
    // Remove old
    QMap<QString, Qt4UiCodeModelSupport *>::const_iterator it, end;
    end = oldCodeModelSupport.constEnd();
    for (it = oldCodeModelSupport.constBegin(); it!=end; ++it) {
        modelManager->removeEditorSupport(it.value());
        delete it.value();
    }
}

Qt4NodesWatcher::Qt4NodesWatcher(QObject *parent)
        : NodesWatcher(parent)
{
}
