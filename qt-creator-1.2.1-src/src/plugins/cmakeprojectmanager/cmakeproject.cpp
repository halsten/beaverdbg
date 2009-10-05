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

#include "cmakeproject.h"
#include "cmakeprojectconstants.h"
#include "cmakeprojectnodes.h"
#include "cmakerunconfiguration.h"
#include "makestep.h"
#include "cmakeopenprojectwizard.h"
#include "cmakebuildenvironmentwidget.h"

#include <projectexplorer/projectexplorerconstants.h>
#include <cpptools/cppmodelmanagerinterface.h>
#include <extensionsystem/pluginmanager.h>
#include <utils/qtcassert.h>
#include <coreplugin/icore.h>

#include <QtCore/QMap>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QDateTime>
#include <QtCore/QProcess>
#include <QtGui/QFormLayout>
#include <QtGui/QMainWindow>

using namespace CMakeProjectManager;
using namespace CMakeProjectManager::Internal;
using ProjectExplorer::Environment;
using ProjectExplorer::EnvironmentItem;

// QtCreator CMake Generator wishlist:
// Which make targets we need to build to get all executables
// What is the make we need to call
// What is the actual compiler executable
// DEFINES

// Open Questions
// Who sets up the environment for cl.exe ? INCLUDEPATH and so on


CMakeProject::CMakeProject(CMakeManager *manager, const QString &fileName)
    : m_manager(manager),
      m_fileName(fileName),
      m_rootNode(new CMakeProjectNode(m_fileName)),
      m_toolChain(0),
      m_insideFileChanged(false)
{
    m_file = new CMakeFile(this, fileName);
}

CMakeProject::~CMakeProject()
{
    delete m_rootNode;
    delete m_toolChain;
}

void CMakeProject::slotActiveBuildConfiguration()
{
    // Pop up a dialog asking the user to rerun cmake
    QFileInfo sourceFileInfo(m_fileName);
    QStringList needToCreate;
    QStringList needToUpdate;

    QString cbpFile = CMakeManager::findCbpFile(QDir(buildDirectory(activeBuildConfiguration())));
    QFileInfo cbpFileFi(cbpFile);
    CMakeOpenProjectWizard::Mode mode;
    if (!cbpFileFi.exists())
        mode = CMakeOpenProjectWizard::NeedToCreate;
    else if (cbpFileFi.lastModified() < sourceFileInfo.lastModified())
        mode = CMakeOpenProjectWizard::NeedToUpdate;
    else
        mode = CMakeOpenProjectWizard::Nothing;

    if (mode != CMakeOpenProjectWizard::Nothing) {
        CMakeOpenProjectWizard copw(m_manager,
                                    sourceFileInfo.absolutePath(),
                                    buildDirectory(activeBuildConfiguration()),
                                    mode,
                                    environment(activeBuildConfiguration()));
        copw.exec();
    }
    // reparse
    parseCMakeLists();
}

void CMakeProject::fileChanged(const QString &fileName)
{
    if (m_insideFileChanged== true)
        return;
    m_insideFileChanged = true;
    if (fileName == m_fileName) {
        // Oh we have changed...
        slotActiveBuildConfiguration();
    }
    m_insideFileChanged = false;
}

void CMakeProject::updateToolChain(const QString &compiler)
{
    //qDebug()<<"CodeBlocks Compilername"<<compiler
    ProjectExplorer::ToolChain *newToolChain = 0;
    if (compiler == "gcc") {
        newToolChain = ProjectExplorer::ToolChain::createGccToolChain("gcc");
    } else if (compiler == "msvc8") {
        // TODO MSVC toolchain
        //newToolChain = ProjectExplorer::ToolChain::createMSVCToolChain("//TODO");
        Q_ASSERT(false);
    } else {
        // TODO other toolchains
        qDebug()<<"Not implemented yet!!! Qt Creator doesn't know which toolchain to use for"<<compiler;
    }

    if (ProjectExplorer::ToolChain::equals(newToolChain, m_toolChain)) {
        delete newToolChain;
        newToolChain = 0;
    } else {
        delete m_toolChain;
        m_toolChain = newToolChain;
    }
}

void CMakeProject::changeBuildDirectory(const QString &buildConfiguration, const QString &newBuildDirectory)
{
    setValue(buildConfiguration, "buildDirectory", newBuildDirectory);
    parseCMakeLists();
}

QString CMakeProject::sourceDirectory() const
{
    return QFileInfo(m_fileName).absolutePath();
}

void CMakeProject::parseCMakeLists()
{
    // Find cbp file
    QString cbpFile = CMakeManager::findCbpFile(buildDirectory(activeBuildConfiguration()));

    // setFolderName
    m_rootNode->setFolderName(QFileInfo(cbpFile).completeBaseName());
    CMakeCbpParser cbpparser;
    // Parsing
    //qDebug()<<"Parsing file "<<cbpFile;
    if (cbpparser.parseCbpFile(cbpFile)) {        
        // ToolChain
        updateToolChain(cbpparser.compilerName());

        m_projectName = cbpparser.projectName();
        m_rootNode->setFolderName(cbpparser.projectName());

        //qDebug()<<"Building Tree";

        QList<ProjectExplorer::FileNode *> fileList = cbpparser.fileList();
        // Manually add the CMakeLists.txt file
        fileList.append(new ProjectExplorer::FileNode(sourceDirectory() + "/CMakeLists.txt", ProjectExplorer::ProjectFileType, false));

        m_files.clear();
        foreach (ProjectExplorer::FileNode *fn, fileList)
            m_files.append(fn->path());
        m_files.sort();

        buildTree(m_rootNode, fileList);


        //qDebug()<<"Adding Targets";
        m_targets = cbpparser.targets();
//        qDebug()<<"Printing targets";
//        foreach(CMakeTarget ct, m_targets) {
//            qDebug()<<ct.title<<" with executable:"<<ct.executable;
//            qDebug()<<"WD:"<<ct.workingDirectory;
//            qDebug()<<ct.makeCommand<<ct.makeCleanCommand;
//            qDebug()<<"";
//        }

        //qDebug()<<"Updating CodeModel";

        QStringList allIncludePaths;
        QStringList allFrameworkPaths;
        QList<ProjectExplorer::HeaderPath> allHeaderPaths = m_toolChain->systemHeaderPaths();
        foreach (ProjectExplorer::HeaderPath headerPath, allHeaderPaths) {
            if (headerPath.kind() == ProjectExplorer::HeaderPath::FrameworkHeaderPath)
                allFrameworkPaths.append(headerPath.path());
            else
                allIncludePaths.append(headerPath.path());
        }
        // This explicitly adds -I. to the include paths
        allIncludePaths.append(sourceDirectory());

        allIncludePaths.append(cbpparser.includeFiles());
        CppTools::CppModelManagerInterface *modelmanager = ExtensionSystem::PluginManager::instance()->getObject<CppTools::CppModelManagerInterface>();
        if (modelmanager) {
            CppTools::CppModelManagerInterface::ProjectInfo pinfo = modelmanager->projectInfo(this);
            pinfo.includePaths = allIncludePaths;
            // TODO we only want C++ files, not all other stuff that might be in the project
            pinfo.sourceFiles = m_files;
            pinfo.defines = m_toolChain->predefinedMacros(); // TODO this is to simplistic
            pinfo.frameworkPaths = allFrameworkPaths;
            modelmanager->updateProjectInfo(pinfo);
        }

        // Create run configurations for m_targets
        //qDebug()<<"Create run configurations of m_targets";
        QMultiMap<QString, QSharedPointer<CMakeRunConfiguration> > existingRunConfigurations;
        foreach(QSharedPointer<ProjectExplorer::RunConfiguration> cmakeRunConfiguration, runConfigurations()) {
            if (QSharedPointer<CMakeRunConfiguration> rc = cmakeRunConfiguration.dynamicCast<CMakeRunConfiguration>()) {
                existingRunConfigurations.insert(rc->title(), rc);
            }
        }

        bool setActive = existingRunConfigurations.isEmpty();
        foreach(const CMakeTarget &ct, m_targets) {
            if (ct.executable.isEmpty())
                continue;
            if (ct.title.endsWith("/fast"))
                continue;
            QList<QSharedPointer<CMakeRunConfiguration> > list = existingRunConfigurations.values(ct.title);
            if (!list.isEmpty()) {
                // Already exists, so override the settings...
                foreach (QSharedPointer<CMakeRunConfiguration> rc, list) {
                    //qDebug()<<"Updating Run Configuration with title"<<ct.title;
                    //qDebug()<<"  Executable new:"<<ct.executable<< "old:"<<rc->executable();
                    //qDebug()<<"  WD new:"<<ct.workingDirectory<<"old:"<<rc->workingDirectory();
                    rc->setExecutable(ct.executable);
                    rc->setWorkingDirectory(ct.workingDirectory);
                }
                existingRunConfigurations.remove(ct.title);
            } else {
                // Does not exist yet
                //qDebug()<<"Adding new run configuration with title"<<ct.title;
                //qDebug()<<"  Executable:"<<ct.executable<<"WD:"<<ct.workingDirectory;
                QSharedPointer<ProjectExplorer::RunConfiguration> rc(new CMakeRunConfiguration(this, ct.executable, ct.workingDirectory, ct.title));
                addRunConfiguration(rc);
                // The first one gets the honour of beeing the active one
                if (setActive) {
                    setActiveRunConfiguration(rc);
                    setActive = false;
                }
            }
        }
        QMultiMap<QString, QSharedPointer<CMakeRunConfiguration> >::const_iterator it =
                existingRunConfigurations.constBegin();
        for( ; it != existingRunConfigurations.constEnd(); ++it) {
            QSharedPointer<CMakeRunConfiguration> rc = it.value();
            //qDebug()<<"Removing old RunConfiguration with title:"<<rc->title();
            //qDebug()<<"  Executable:"<<rc->executable()<<rc->workingDirectory();
            removeRunConfiguration(rc);
        }
        //qDebug()<<"\n";
    } else {
        // TODO report error
        qDebug()<<"Parsing failed";
        delete m_toolChain;
        m_toolChain = 0;
    }
}

QString CMakeProject::buildParser(const QString &buildConfiguration) const
{
    Q_UNUSED(buildConfiguration);
    // TODO this is actually slightly wrong, but do i care?
    // this should call toolchain(buildConfiguration)
    if (!m_toolChain)
        return QString::null;
    if (m_toolChain->type() == ProjectExplorer::ToolChain::GCC
        || m_toolChain->type() == ProjectExplorer::ToolChain::LinuxICC
        || m_toolChain->type() == ProjectExplorer::ToolChain::MinGW) {
        return ProjectExplorer::Constants::BUILD_PARSER_GCC;
    } else if (m_toolChain->type() == ProjectExplorer::ToolChain::MSVC
               || m_toolChain->type() == ProjectExplorer::ToolChain::WINCE) {
        return ProjectExplorer::Constants::BUILD_PARSER_MSVC;
    }
    return QString::null;
}

QStringList CMakeProject::targets() const
{
    QStringList results;
    foreach (const CMakeTarget &ct, m_targets) {
        if (ct.executable.isEmpty())
            continue;
        if (ct.title.endsWith("/fast"))
            continue;
        results << ct.title;
    }
    return results;
}

void CMakeProject::gatherFileNodes(ProjectExplorer::FolderNode *parent, QList<ProjectExplorer::FileNode *> &list)
{
    foreach(ProjectExplorer::FolderNode *folder, parent->subFolderNodes())
        gatherFileNodes(folder, list);
    foreach(ProjectExplorer::FileNode *file, parent->fileNodes())
        list.append(file);
}

void CMakeProject::buildTree(CMakeProjectNode *rootNode, QList<ProjectExplorer::FileNode *> newList)
{
    // Gather old list
    QList<ProjectExplorer::FileNode *> oldList;
    gatherFileNodes(rootNode, oldList);
    qSort(oldList.begin(), oldList.end(), ProjectExplorer::ProjectNode::sortNodesByPath);
    qSort(newList.begin(), newList.end(), ProjectExplorer::ProjectNode::sortNodesByPath);

    // generate added and deleted list
    QList<ProjectExplorer::FileNode *>::const_iterator oldIt  = oldList.constBegin();
    QList<ProjectExplorer::FileNode *>::const_iterator oldEnd = oldList.constEnd();
    QList<ProjectExplorer::FileNode *>::const_iterator newIt  = newList.constBegin();
    QList<ProjectExplorer::FileNode *>::const_iterator newEnd = newList.constEnd();

    QList<ProjectExplorer::FileNode *> added;
    QList<ProjectExplorer::FileNode *> deleted;


    while(oldIt != oldEnd && newIt != newEnd) {
        if ( (*oldIt)->path() == (*newIt)->path()) {
            delete *newIt;
            ++oldIt;
            ++newIt;
        } else if ((*oldIt)->path() < (*newIt)->path()) {
            deleted.append(*oldIt);
            ++oldIt;
        } else {
            added.append(*newIt);
            ++newIt;
        }
    }

    while (oldIt != oldEnd) {
        deleted.append(*oldIt);
        ++oldIt;
    }

    while (newIt != newEnd) {
        added.append(*newIt);
        ++newIt;
    }

    // add added nodes
    foreach (ProjectExplorer::FileNode *fn, added) {
//        qDebug()<<"added"<<fn->path();
        // Get relative path to rootNode
        QString parentDir = QFileInfo(fn->path()).absolutePath();
        ProjectExplorer::FolderNode *folder = findOrCreateFolder(rootNode, parentDir);
        rootNode->addFileNodes(QList<ProjectExplorer::FileNode *>()<< fn, folder);
    }

    // remove old file nodes and check wheter folder nodes can be removed
    foreach (ProjectExplorer::FileNode *fn, deleted) {
        ProjectExplorer::FolderNode *parent = fn->parentFolderNode();
//        qDebug()<<"removed"<<fn->path();
        rootNode->removeFileNodes(QList<ProjectExplorer::FileNode *>() << fn, parent);
        // Check for empty parent
        while (parent->subFolderNodes().isEmpty() && parent->fileNodes().isEmpty()) {
            ProjectExplorer::FolderNode *grandparent = parent->parentFolderNode();
            rootNode->removeFolderNodes(QList<ProjectExplorer::FolderNode *>() << parent, grandparent);
            parent = grandparent;
            if (parent == rootNode)
                break;
        }
    }
}

ProjectExplorer::FolderNode *CMakeProject::findOrCreateFolder(CMakeProjectNode *rootNode, QString directory)
{
    QString relativePath = QDir(QFileInfo(rootNode->path()).path()).relativeFilePath(directory);
    QStringList parts = relativePath.split("/", QString::SkipEmptyParts);
    ProjectExplorer::FolderNode *parent = rootNode;
    foreach (const QString &part, parts) {
        // Find folder in subFolders
        bool found = false;
        foreach (ProjectExplorer::FolderNode *folder, parent->subFolderNodes()) {
            if (QFileInfo(folder->path()).fileName() == part) {
                // yeah found something :)
                parent = folder;
                found = true;
                break;
            }
        }
        if (!found) {
            // No FolderNode yet, so create it
            ProjectExplorer::FolderNode *tmp = new ProjectExplorer::FolderNode(part);
            rootNode->addFolderNodes(QList<ProjectExplorer::FolderNode *>() << tmp, parent);
            parent = tmp;
        }
    }
    return parent;
}

QString CMakeProject::name() const
{
    return m_projectName;
}



Core::IFile *CMakeProject::file() const
{
    return m_file;
}

CMakeManager *CMakeProject::projectManager() const
{
    return m_manager;
}

QList<ProjectExplorer::Project *> CMakeProject::dependsOn()
{
    return QList<Project *>();
}

bool CMakeProject::isApplication() const
{
    return true;
}

ProjectExplorer::Environment CMakeProject::baseEnvironment(const QString &buildConfiguration) const
{
    Environment env = useSystemEnvironment(buildConfiguration) ? Environment(QProcess::systemEnvironment()) : Environment();
    return env;
}

ProjectExplorer::Environment CMakeProject::environment(const QString &buildConfiguration) const
{
    Environment env = baseEnvironment(buildConfiguration);
    env.modify(userEnvironmentChanges(buildConfiguration));
    return env;
}

void CMakeProject::setUseSystemEnvironment(const QString &buildConfiguration, bool b)
{
    if (b == useSystemEnvironment(buildConfiguration))
        return;
    setValue(buildConfiguration, "clearSystemEnvironment", !b);
    emit environmentChanged(buildConfiguration);
}

bool CMakeProject::useSystemEnvironment(const QString &buildConfiguration) const
{
    bool b = !(value(buildConfiguration, "clearSystemEnvironment").isValid() && value(buildConfiguration, "clearSystemEnvironment").toBool());
    return b;
}

QList<ProjectExplorer::EnvironmentItem> CMakeProject::userEnvironmentChanges(const QString &buildConfig) const
{
    return EnvironmentItem::fromStringList(value(buildConfig, "userEnvironmentChanges").toStringList());
}

void CMakeProject::setUserEnvironmentChanges(const QString &buildConfig, const QList<ProjectExplorer::EnvironmentItem> &diff)
{
    QStringList list = EnvironmentItem::toStringList(diff);
    if (list == value(buildConfig, "userEnvironmentChanges"))
        return;
    setValue(buildConfig, "userEnvironmentChanges", EnvironmentItem::toStringList(diff));
    emit environmentChanged(buildConfig);
}

QString CMakeProject::buildDirectory(const QString &buildConfiguration) const
{
    QString buildDirectory = value(buildConfiguration, "buildDirectory").toString();
    if (buildDirectory.isEmpty())
        buildDirectory = sourceDirectory() + "/qtcreator-build";
    return buildDirectory;
}

ProjectExplorer::BuildStepConfigWidget *CMakeProject::createConfigWidget()
{
    return new CMakeBuildSettingsWidget(this);
}

QList<ProjectExplorer::BuildStepConfigWidget*> CMakeProject::subConfigWidgets()
{
    QList<ProjectExplorer::BuildStepConfigWidget*> list;
    list <<  new CMakeBuildEnvironmentWidget(this);
    return list;
}

// This method is called for new build configurations
// You should probably set some default values in this method
 void CMakeProject::newBuildConfiguration(const QString &buildConfiguration)
 {
     // Default to all
     makeStep()->setBuildTarget(buildConfiguration, "all", true);

    CMakeOpenProjectWizard copw(projectManager(), sourceDirectory(), buildDirectory(buildConfiguration), environment(buildConfiguration));
    if (copw.exec() == QDialog::Accepted) {
        setValue(buildConfiguration, "buildDirectory", copw.buildDirectory());
        parseCMakeLists();
    }
 }

ProjectExplorer::ProjectNode *CMakeProject::rootProjectNode() const
{
    return m_rootNode;
}


QStringList CMakeProject::files(FilesMode fileMode) const
{
    Q_UNUSED(fileMode);
    return m_files;
}

void CMakeProject::saveSettingsImpl(ProjectExplorer::PersistentSettingsWriter &writer)
{
    Project::saveSettingsImpl(writer);
}

MakeStep *CMakeProject::makeStep() const
{
    foreach (ProjectExplorer::BuildStep *bs, buildSteps()) {
        MakeStep *ms = qobject_cast<MakeStep *>(bs);
        if (ms)
            return ms;
    }
    return 0;
}


void CMakeProject::restoreSettingsImpl(ProjectExplorer::PersistentSettingsReader &reader)
{
    Project::restoreSettingsImpl(reader);
    bool hasUserFile = !buildConfigurations().isEmpty();
    if (!hasUserFile) {
        // Ask the user for where he wants to build it
        // and the cmake command line

        CMakeOpenProjectWizard copw(m_manager, sourceDirectory(), ProjectExplorer::Environment::systemEnvironment());
        copw.exec();
        // TODO handle cancel....

        qDebug()<<"ccd.buildDirectory()"<<copw.buildDirectory();

        // Now create a standard build configuration
        MakeStep *makeStep = new MakeStep(this);

        insertBuildStep(0, makeStep);

        addBuildConfiguration("all");
        makeStep->setBuildTarget("all", "all", true);
        if (!copw.buildDirectory().isEmpty())
            setValue("all", "buildDirectory", copw.buildDirectory());
        //TODO save arguments somewhere copw.arguments()

        MakeStep *cleanMakeStep = new MakeStep(this);
        insertCleanStep(0, cleanMakeStep);
        cleanMakeStep->setValue("clean", true);
        setActiveBuildConfiguration("all");

    } else {
        // We have a user file, but we could still be missing the cbp file
        // or simply run createXml with the saved settings
        QFileInfo sourceFileInfo(m_fileName);
        QStringList needToCreate;
        QStringList needToUpdate;
        QString cbpFile = CMakeManager::findCbpFile(QDir(buildDirectory(activeBuildConfiguration())));
        QFileInfo cbpFileFi(cbpFile);

        CMakeOpenProjectWizard::Mode mode = CMakeOpenProjectWizard::Nothing;
        if (!cbpFileFi.exists())
            mode = CMakeOpenProjectWizard::NeedToCreate;
        else if (cbpFileFi.lastModified() < sourceFileInfo.lastModified())
            mode = CMakeOpenProjectWizard::NeedToUpdate;

        if (mode != CMakeOpenProjectWizard::Nothing) {
            CMakeOpenProjectWizard copw(m_manager,
                                        sourceFileInfo.absolutePath(),
                                        buildDirectory(activeBuildConfiguration()),
                                        mode,
                                        environment(activeBuildConfiguration()));
            copw.exec();
        }
    }
    parseCMakeLists(); // Gets the directory from the active buildconfiguration

    m_watcher = new ProjectExplorer::FileWatcher(this);
    connect(m_watcher, SIGNAL(fileChanged(QString)), this, SLOT(fileChanged(QString)));
    m_watcher->addFile(m_fileName);

    connect(this, SIGNAL(activeBuildConfigurationChanged()),
            this, SLOT(slotActiveBuildConfiguration()));
}

CMakeTarget CMakeProject::targetForTitle(const QString &title)
{
    foreach(const CMakeTarget &ct, m_targets)
        if (ct.title == title)
            return ct;
    return CMakeTarget();
}

CMakeFile::CMakeFile(CMakeProject *parent, QString fileName)
    : Core::IFile(parent), m_project(parent), m_fileName(fileName)
{

}

bool CMakeFile::save(const QString &fileName)
{
    // Once we have an texteditor open for this file, we probably do
    // need to implement this, don't we.
    Q_UNUSED(fileName);
    return false;
}

QString CMakeFile::fileName() const
{
    return m_fileName;
}

QString CMakeFile::defaultPath() const
{
    return QString();
}

QString CMakeFile::suggestedFileName() const
{
    return QString();
}

QString CMakeFile::mimeType() const
{
    return Constants::CMAKEMIMETYPE;
}


bool CMakeFile::isModified() const
{
    return false;
}

bool CMakeFile::isReadOnly() const
{
    return true;
}

bool CMakeFile::isSaveAsAllowed() const
{
    return false;
}

void CMakeFile::modified(ReloadBehavior *behavior)
{
    Q_UNUSED(behavior);
}

CMakeBuildSettingsWidget::CMakeBuildSettingsWidget(CMakeProject *project)
    : m_project(project)
{
    QFormLayout *fl = new QFormLayout(this);
    setLayout(fl);
    m_pathLineEdit = new QLineEdit(this);
    m_pathLineEdit->setReadOnly(true);
    // TODO currently doesn't work
    // since creating the cbp file also creates makefiles
    // and then cmake builds in that directory instead of shadow building
    // We need our own generator for that to work

    QHBoxLayout *hbox = new QHBoxLayout();
    hbox->addWidget(m_pathLineEdit);

    m_changeButton = new QPushButton(this);
    m_changeButton->setText(tr("&Change"));
    connect(m_changeButton, SIGNAL(clicked()), this, SLOT(openChangeBuildDirectoryDialog()));
    hbox->addWidget(m_changeButton);

    fl->addRow("Build directory:", hbox);
}

QString CMakeBuildSettingsWidget::displayName() const
{
    return "CMake";
}

void CMakeBuildSettingsWidget::init(const QString &buildConfiguration)
{
    m_buildConfiguration = buildConfiguration;
    m_pathLineEdit->setText(m_project->buildDirectory(buildConfiguration));
    if (m_project->buildDirectory(buildConfiguration) == m_project->sourceDirectory())
        m_changeButton->setEnabled(false);
    else
        m_changeButton->setEnabled(true);
}

void CMakeBuildSettingsWidget::openChangeBuildDirectoryDialog()
{
    CMakeOpenProjectWizard copw(m_project->projectManager(),
                                m_project->sourceDirectory(),
                                m_project->buildDirectory(m_buildConfiguration),
                                m_project->environment(m_buildConfiguration));
    if (copw.exec() == QDialog::Accepted) {
        m_project->changeBuildDirectory(m_buildConfiguration, copw.buildDirectory());
        m_pathLineEdit->setText(m_project->buildDirectory(m_buildConfiguration));
    }
}

/////
// CMakeCbpParser
////

bool CMakeCbpParser::parseCbpFile(const QString &fileName)
{
    QFile fi(fileName);
    if (fi.exists() && fi.open(QFile::ReadOnly)) {
        setDevice(&fi);

        while (!atEnd()) {
            readNext();
            if (name() == "CodeBlocks_project_file") {
                parseCodeBlocks_project_file();
            } else if (isStartElement()) {
                parseUnknownElement();
            }
        }
        fi.close();
        m_includeFiles.sort();
        m_includeFiles.removeDuplicates();
        return true;
    }
    return false;
}

void CMakeCbpParser::parseCodeBlocks_project_file()
{
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if (name() == "Project") {
            parseProject();
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseProject()
{
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if (name() == "Option") {
            parseOption();
        } else if (name() == "Unit") {
            parseUnit();
        } else if (name() == "Build") {
            parseBuild();
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseBuild()
{
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if (name() == "Target") {
            parseTarget();
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseTarget()
{
    m_targetType = false;
    m_target.clear();

    if (attributes().hasAttribute("title"))
        m_target.title = attributes().value("title").toString();
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            if (m_targetType || m_target.title == "all" || m_target.title == "install") {
                m_targets.append(m_target);
            }
            return;
        } else if (name() == "Compiler") {
            parseCompiler();
        } else if (name() == "Option") {
            parseTargetOption();
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseTargetOption()
{
    if (attributes().hasAttribute("output"))
        m_target.executable = attributes().value("output").toString();
    else if (attributes().hasAttribute("type") && (attributes().value("type") == "1" || attributes().value("type") == "0"))
        m_targetType = true;
    else if (attributes().hasAttribute("working_dir"))
        m_target.workingDirectory = attributes().value("working_dir").toString();
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if (name() == "MakeCommand") {
            parseMakeCommand();
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

QString CMakeCbpParser::projectName() const
{
    return m_projectName;
}

void CMakeCbpParser::parseOption()
{
    if (attributes().hasAttribute("title"))
        m_projectName = attributes().value("title").toString();

    if (attributes().hasAttribute("compiler"))
        m_compiler = attributes().value("compiler").toString();

    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if(isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseMakeCommand()
{
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if (name() == "Build") {
            parseTargetBuild();
        } else if (name() == "Clean") {
            parseTargetClean();
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseTargetBuild()
{
    if (attributes().hasAttribute("command"))
        m_target.makeCommand = attributes().value("command").toString();
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseTargetClean()
{
    if (attributes().hasAttribute("command"))
        m_target.makeCleanCommand = attributes().value("command").toString();
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseCompiler()
{
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if (name() == "Add") {
            parseAdd();
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseAdd()
{
    m_includeFiles.append(attributes().value("directory").toString());
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseUnit()
{
    //qDebug()<<stream.attributes().value("filename");
    QString fileName = attributes().value("filename").toString();
    if (!fileName.endsWith(".rule"))
        m_fileList.append( new ProjectExplorer::FileNode(fileName, ProjectExplorer::SourceType, false));
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            return;
        } else if (isStartElement()) {
            parseUnknownElement();
        }
    }
}

void CMakeCbpParser::parseUnknownElement()
{
    Q_ASSERT(isStartElement());

    while (!atEnd()) {
        readNext();

        if (isEndElement())
            break;

        if (isStartElement())
            parseUnknownElement();
    }
}

QList<ProjectExplorer::FileNode *> CMakeCbpParser::fileList()
{
    return m_fileList;
}

QStringList CMakeCbpParser::includeFiles()
{
    return m_includeFiles;
}

QList<CMakeTarget> CMakeCbpParser::targets()
{
    return m_targets;
}

QString CMakeCbpParser::compilerName() const
{
    return m_compiler;
}

void CMakeTarget::clear()
{
    executable = QString::null;
    makeCommand = QString::null;
    makeCleanCommand = QString::null;
    workingDirectory = QString::null;
    title = QString::null;
}

