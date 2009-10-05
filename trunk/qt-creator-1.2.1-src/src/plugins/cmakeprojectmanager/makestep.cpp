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

#include "makestep.h"
#include "cmakeprojectconstants.h"
#include "cmakeproject.h"

#include <QtGui/QFormLayout>
#include <QtGui/QGroupBox>
#include <QtGui/QCheckBox>
#include <QtGui/QLineEdit>
#include <QtGui/QListWidget>

using namespace CMakeProjectManager;
using namespace CMakeProjectManager::Internal;

MakeStep::MakeStep(CMakeProject *pro)
    : AbstractMakeStep(pro), m_pro(pro)
{
    m_percentProgress = QRegExp("^\\[\\s*(\\d*)%\\]");
}

MakeStep::~MakeStep()
{

}

bool MakeStep::init(const QString &buildConfiguration)
{
    setBuildParser(m_pro->buildParser(buildConfiguration));

    setEnabled(buildConfiguration, true);
    setWorkingDirectory(buildConfiguration, m_pro->buildDirectory(buildConfiguration));
#ifdef Q_OS_WIN
    setCommand(buildConfiguration, "mingw32-make");
#else // Q_OS_WIN
    setCommand(buildConfiguration, "make"); // TODO give full path here?
#endif // Q_OS_WIN

    if (value("clean").isValid() && value("clean").toBool())  {
       setArguments(buildConfiguration, QStringList() << "clean");
   } else {
        QStringList arguments = value(buildConfiguration, "buildTargets").toStringList();
        arguments << additionalArguments(buildConfiguration);
        setArguments(buildConfiguration, arguments); // TODO
        setEnvironment(buildConfiguration, m_pro->environment(buildConfiguration));
    }
    return AbstractMakeStep::init(buildConfiguration);
}

void MakeStep::run(QFutureInterface<bool> &fi)
{
    m_futureInterface = &fi;
    m_futureInterface->setProgressRange(0, 100);
    AbstractMakeStep::run(fi);
    m_futureInterface->setProgressValue(100);
    m_futureInterface->reportFinished();
    m_futureInterface = 0;
}

QString MakeStep::name()
{
    return Constants::MAKESTEP;
}

QString MakeStep::displayName()
{
    return "Make";
}

ProjectExplorer::BuildStepConfigWidget *MakeStep::createConfigWidget()
{
    return new MakeStepConfigWidget(this);
}

bool MakeStep::immutable() const
{
    return true;
}

void MakeStep::stdOut(const QString &line)
{
    if (m_percentProgress.indexIn(line) != -1) {
        bool ok = false;
        int percent = m_percentProgress.cap(1).toInt(&ok);;
        if (ok)
            m_futureInterface->setProgressValue(percent);
    }
    AbstractMakeStep::stdOut(line);
}

CMakeProject *MakeStep::project() const
{
    return m_pro;
}

bool MakeStep::buildsTarget(const QString &buildConfiguration, const QString &target) const
{
    return value(buildConfiguration, "buildTargets").toStringList().contains(target);
}

void MakeStep::setBuildTarget(const QString &buildConfiguration, const QString &target, bool on)
{
    QStringList old = value(buildConfiguration, "buildTargets").toStringList();
    if (on && !old.contains(target))
        setValue(buildConfiguration, "buildTargets", old << target);
    else if(!on && old.contains(target))
        setValue(buildConfiguration, "buildTargets", old.removeOne(target));
}

QStringList MakeStep::additionalArguments(const QString &buildConfiguration) const
{
    return value(buildConfiguration, "additionalArguments").toStringList();
}

void MakeStep::setAdditionalArguments(const QString &buildConfiguration, const QStringList &list)
{
    setValue(buildConfiguration, "additionalArguments", list);
}

//
// MakeStepConfigWidget
//

MakeStepConfigWidget::MakeStepConfigWidget(MakeStep *makeStep)
    : m_makeStep(makeStep)
{
    QFormLayout *fl = new QFormLayout(this);
    setLayout(fl);

    m_additionalArguments = new QLineEdit(this);
    fl->addRow(tr("Additional arguments:"), m_additionalArguments);

    connect(m_additionalArguments, SIGNAL(textEdited(const QString &)), this, SLOT(additionalArgumentsEdited()));

    m_targetsList = new QListWidget;
    fl->addRow(tr("Targets:"), m_targetsList);

    // TODO update this list also on rescans of the CMakeLists.txt
    CMakeProject *pro = m_makeStep->project();
    foreach(const QString& target, pro->targets()) {
        QListWidgetItem *item = new QListWidgetItem(target, m_targetsList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }

    connect(m_targetsList, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(itemChanged(QListWidgetItem*)));
}

void MakeStepConfigWidget::additionalArgumentsEdited()
{
    m_makeStep->setAdditionalArguments(m_buildConfiguration, ProjectExplorer::Environment::parseCombinedArgString(m_additionalArguments->text()));
}

void MakeStepConfigWidget::itemChanged(QListWidgetItem *item)
{
    m_makeStep->setBuildTarget(m_buildConfiguration, item->text(), item->checkState() & Qt::Checked);
}

QString MakeStepConfigWidget::displayName() const
{
    return "Make";
}

void MakeStepConfigWidget::init(const QString &buildConfiguration)
{
    // disconnect to make the changes to the items
    disconnect(m_targetsList, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(itemChanged(QListWidgetItem*)));
    m_buildConfiguration = buildConfiguration;
    int count = m_targetsList->count();
    for(int i = 0; i < count; ++i) {
        QListWidgetItem *item = m_targetsList->item(i);
        item->setCheckState(m_makeStep->buildsTarget(buildConfiguration, item->text()) ? Qt::Checked : Qt::Unchecked);
    }
    // and connect again
    connect(m_targetsList, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(itemChanged(QListWidgetItem*)));

    m_additionalArguments->setText(ProjectExplorer::Environment::joinArgumentList(m_makeStep->additionalArguments(m_buildConfiguration)));
}

//
// MakeStepFactory
//

bool MakeStepFactory::canCreate(const QString &name) const
{
    return (Constants::MAKESTEP == name);
}

ProjectExplorer::BuildStep *MakeStepFactory::create(ProjectExplorer::Project *project, const QString &name) const
{
    Q_ASSERT(name == Constants::MAKESTEP);
    CMakeProject *pro = qobject_cast<CMakeProject *>(project);
    Q_ASSERT(pro);
    return new MakeStep(pro);
}

QStringList MakeStepFactory::canCreateForProject(ProjectExplorer::Project * /* pro */) const
{
    return QStringList();
}

QString MakeStepFactory::displayNameForName(const QString & /* name */) const
{
    return "Make";
}

