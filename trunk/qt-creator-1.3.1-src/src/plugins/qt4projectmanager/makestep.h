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

#ifndef MAKESTEP_H
#define MAKESTEP_H

#include "ui_makestep.h"
#include "qtversionmanager.h"

#include <projectexplorer/abstractmakestep.h>
#include <projectexplorer/projectexplorer.h>

namespace ProjectExplorer {
class BuildStep;
class IBuildStepFactory;
class Project;
}

namespace Qt4ProjectManager {

namespace Internal {
class MakeStepFactory : public ProjectExplorer::IBuildStepFactory
{
    Q_OBJECT
public:
    MakeStepFactory();
    virtual ~MakeStepFactory();
    bool canCreate(const QString & name) const;
    ProjectExplorer::BuildStep * create(ProjectExplorer::Project * pro, const QString & name) const;
    QStringList canCreateForProject(ProjectExplorer::Project *pro) const;
    QString displayNameForName(const QString &name) const;
};
}

class Qt4Project;

class MakeStep : public ProjectExplorer::AbstractMakeStep
{
    Q_OBJECT
public:
    MakeStep(Qt4Project * project);
    ~MakeStep();
    virtual bool init(const QString & name);
    virtual void run(QFutureInterface<bool> &);
    virtual QString name();
    virtual QString displayName();
    virtual ProjectExplorer::BuildStepConfigWidget *createConfigWidget();
    virtual bool immutable() const;
    void setMakeArguments(const QString &buildConfiguration, const QStringList &arguments);
signals:
    void changed();
private:
    QString m_buildConfiguration;
};

class MakeStepConfigWidget : public ProjectExplorer::BuildStepConfigWidget
{
    Q_OBJECT
public:
    MakeStepConfigWidget(MakeStep *makeStep);
    QString displayName() const;
    void init(const QString &buildConfiguration);
    QString summaryText() const;
private slots:
    void makeLineEditTextEdited();
    void makeArgumentsLineEditTextEdited();
    void update();
    void updateMakeOverrideLabel();
    void updateDetails();
private:
    QString m_buildConfiguration;
    Ui::MakeStep m_ui;
    MakeStep *m_makeStep;
    QString m_summaryText;
};

} // Qt4ProjectManager

#endif // MAKESTEP_H
