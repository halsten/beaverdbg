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

#ifndef GENERICPROJECT_H
#define GENERICPROJECT_H

#include "genericprojectmanager.h"
#include "genericprojectnodes.h"

#include <projectexplorer/project.h>
#include <projectexplorer/projectnodes.h>
#include <projectexplorer/buildstep.h>
#include <coreplugin/ifile.h>

QT_BEGIN_NAMESPACE
class QPushButton;
class QStringListModel;
QT_END_NAMESPACE

namespace Core {
namespace Utils {
class PathChooser;
}
}

namespace ProjectExplorer {
class ToolChain;
}

namespace GenericProjectManager {
namespace Internal {

class GenericMakeStep;
class GenericProjectFile;

class GenericProject : public ProjectExplorer::Project
{
    Q_OBJECT

public:
    GenericProject(Manager *manager, const QString &filename);
    virtual ~GenericProject();

    QString filesFileName() const;
    QString includesFileName() const;
    QString configFileName() const;

    virtual QString name() const;
    virtual Core::IFile *file() const;
    virtual ProjectExplorer::IProjectManager *projectManager() const;

    virtual QList<ProjectExplorer::Project *> dependsOn();

    virtual bool isApplication() const;

    virtual ProjectExplorer::Environment environment(const QString &buildConfiguration) const;
    virtual QString buildDirectory(const QString &buildConfiguration) const;

    virtual ProjectExplorer::BuildStepConfigWidget *createConfigWidget();
    virtual QList<ProjectExplorer::BuildStepConfigWidget*> subConfigWidgets();

    virtual void newBuildConfiguration(const QString &buildConfiguration);
    virtual GenericProjectNode *rootProjectNode() const;
    virtual QStringList files(FilesMode fileMode) const;

    QStringList targets() const;
    GenericMakeStep *makeStep() const;
    QString buildParser(const QString &buildConfiguration) const;
    ProjectExplorer::ToolChain *toolChain() const;

    enum RefreshOptions {
        Files         = 0x01,
        Configuration = 0x02,
        Everything    = Files | Configuration
    };

    void refresh(RefreshOptions options);

    QStringList includePaths() const;
    void setIncludePaths(const QStringList &includePaths);

    QByteArray defines() const;
    QStringList allIncludePaths() const;
    QStringList projectIncludePaths() const;
    QStringList files() const;
    QStringList generated() const;    
    QString toolChainId() const;

public Q_SLOTS:
    void setToolChainId(const QString &toolChainId);

protected:
    virtual void saveSettingsImpl(ProjectExplorer::PersistentSettingsWriter &writer);
    virtual void restoreSettingsImpl(ProjectExplorer::PersistentSettingsReader &reader);

private:
    void parseProject(RefreshOptions options);
    QStringList convertToAbsoluteFiles(const QStringList &paths) const;

    Manager *m_manager;
    QString m_fileName;
    QString m_filesFileName;
    QString m_includesFileName;
    QString m_configFileName;
    GenericProjectFile *m_file;
    QString m_projectName;

    QStringList m_files;
    QStringList m_generated;
    QStringList m_includePaths;
    QStringList m_projectIncludePaths;
    QByteArray m_defines;

    GenericProjectNode *m_rootNode;
    ProjectExplorer::ToolChain *m_toolChain;
    QString m_toolChainId;
};

class GenericProjectFile : public Core::IFile
{
    Q_OBJECT

public:
    GenericProjectFile(GenericProject *parent, QString fileName);
    virtual ~GenericProjectFile();

    virtual bool save(const QString &fileName = QString());
    virtual QString fileName() const;

    virtual QString defaultPath() const;
    virtual QString suggestedFileName() const;
    virtual QString mimeType() const;

    virtual bool isModified() const;
    virtual bool isReadOnly() const;
    virtual bool isSaveAsAllowed() const;

    virtual void modified(ReloadBehavior *behavior);

private:
    GenericProject *m_project;
    QString m_fileName;
};

class GenericBuildSettingsWidget : public ProjectExplorer::BuildStepConfigWidget
{
    Q_OBJECT

public:
    GenericBuildSettingsWidget(GenericProject *project);
    virtual ~GenericBuildSettingsWidget();

    virtual QString displayName() const;

    virtual void init(const QString &buildConfiguration);

private Q_SLOTS:
    void buildDirectoryChanged();

private:
    GenericProject *m_project;
    Core::Utils::PathChooser *m_pathChooser;
    QString m_buildConfiguration;
};

} // namespace Internal
} // namespace GenericProjectManager

#endif // GENERICPROJECT_H
