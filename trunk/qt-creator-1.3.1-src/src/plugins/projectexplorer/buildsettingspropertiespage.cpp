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

#include "buildsettingspropertiespage.h"
#include "buildstep.h"
#include "buildstepspage.h"
#include "project.h"
#include "buildconfiguration.h"

#include <coreplugin/coreconstants.h>
#include <extensionsystem/pluginmanager.h>
#include <utils/qtcassert.h>

#include <QtCore/QDebug>
#include <QtCore/QPair>
#include <QtGui/QInputDialog>
#include <QtGui/QLabel>
#include <QtGui/QVBoxLayout>
#include <QtGui/QMenu>

using namespace ProjectExplorer;
using namespace ProjectExplorer::Internal;

///
/// BuildSettingsPanelFactory
///

bool BuildSettingsPanelFactory::supports(Project *project)
{
    return project->hasBuildSettings();
}

PropertiesPanel *BuildSettingsPanelFactory::createPanel(Project *project)
{
    return new BuildSettingsPanel(project);
}

///
/// BuildSettingsPanel
///

BuildSettingsPanel::BuildSettingsPanel(Project *project)
        : PropertiesPanel(),
          m_widget(new BuildSettingsWidget(project))
{
}

BuildSettingsPanel::~BuildSettingsPanel()
{

}

QString BuildSettingsPanel::name() const
{
    return tr("Build Settings");
}

QWidget *BuildSettingsPanel::widget()
{
    return m_widget;
}

///
// BuildSettingsSubWidgets
///

BuildSettingsSubWidgets::~BuildSettingsSubWidgets()
{
    clear();
}

void BuildSettingsSubWidgets::addWidget(const QString &name, QWidget *widget)
{
    QSpacerItem *item = new QSpacerItem(1, 10, QSizePolicy::Fixed, QSizePolicy::Fixed);

    QLabel *label = new QLabel(this);
    label->setText(name);
    QFont f = label->font();
    f.setBold(true);
    f.setPointSizeF(f.pointSizeF() *1.2);
    label->setFont(f);

    layout()->addItem(item);
    layout()->addWidget(label);
    layout()->addWidget(widget);

    m_spacerItems.append(item);
    m_labels.append(label);
    m_widgets.append(widget);
}

void BuildSettingsSubWidgets::clear()
{
    foreach(QSpacerItem *item, m_spacerItems)
        layout()->removeItem(item);
    qDeleteAll(m_spacerItems);
    qDeleteAll(m_widgets);
    qDeleteAll(m_labels);
    m_widgets.clear();
    m_labels.clear();
    m_spacerItems.clear();
}

QList<QWidget *> BuildSettingsSubWidgets::widgets() const
{
    return m_widgets;
}

BuildSettingsSubWidgets::BuildSettingsSubWidgets(QWidget *parent)
    : QWidget(parent)
{
    new QVBoxLayout(this);
    layout()->setMargin(0);
}

///
/// BuildSettingsWidget
///

BuildSettingsWidget::~BuildSettingsWidget()
{
}

BuildSettingsWidget::BuildSettingsWidget(Project *project)
    : m_project(project)
{
    QVBoxLayout *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0, -1, 0, -1);

    { // Edit Build Configuration row
        QHBoxLayout *hbox = new QHBoxLayout();
        hbox->addWidget(new QLabel(tr("Edit Build Configuration:"), this));
        m_buildConfigurationComboBox = new QComboBox(this);
        m_buildConfigurationComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        hbox->addWidget(m_buildConfigurationComboBox);

        m_addButton = new QPushButton(this);
        m_addButton->setText(tr("Add"));
        m_addButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        hbox->addWidget(m_addButton);

        m_removeButton = new QPushButton(this);
        m_removeButton->setText(tr("Remove"));
        m_removeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        hbox->addWidget(m_removeButton);
        hbox->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Fixed));
        vbox->addLayout(hbox);
    }

    m_makeActiveLabel = new QLabel(this);
    m_makeActiveLabel->setVisible(false);
    vbox->addWidget(m_makeActiveLabel);

    m_subWidgets = new BuildSettingsSubWidgets(this);
    vbox->addWidget(m_subWidgets);

    m_addButtonMenu = new QMenu(this);
    m_addButton->setMenu(m_addButtonMenu);
    updateAddButtonMenu();

    m_buildConfiguration = m_project->activeBuildConfiguration()->name();

    connect(m_makeActiveLabel, SIGNAL(linkActivated(QString)),
            this, SLOT(makeActive()));

    connect(m_buildConfigurationComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(currentIndexChanged(int)));

    connect(m_removeButton, SIGNAL(clicked()),
            this, SLOT(deleteConfiguration()));

    connect(m_project, SIGNAL(buildConfigurationDisplayNameChanged(const QString &)),
            this, SLOT(buildConfigurationDisplayNameChanged(const QString &)));

    connect(m_project, SIGNAL(activeBuildConfigurationChanged()),
            this, SLOT(checkMakeActiveLabel()));

    if (m_project->buildConfigurationFactory())
        connect(m_project->buildConfigurationFactory(), SIGNAL(availableCreationTypesChanged()), SLOT(updateAddButtonMenu()));

    updateBuildSettings();
}

void BuildSettingsWidget::makeActive()
{
    m_project->setActiveBuildConfiguration(m_project->buildConfiguration(m_buildConfiguration));
}

void BuildSettingsWidget::updateAddButtonMenu()
{
    m_addButtonMenu->clear();
    m_addButtonMenu->addAction(tr("&Clone Selected"),
                             this, SLOT(cloneConfiguration()));
    IBuildConfigurationFactory *factory = m_project->buildConfigurationFactory();
    if (factory) {
        foreach (const QString &type, factory->availableCreationTypes()) {
            QAction *action = m_addButtonMenu->addAction(factory->displayNameForType(type), this, SLOT(createConfiguration()));
            action->setData(type);
        }
    }
}

void BuildSettingsWidget::buildConfigurationDisplayNameChanged(const QString &buildConfiguration)
{
    for (int i=0; i<m_buildConfigurationComboBox->count(); ++i) {
        if (m_buildConfigurationComboBox->itemData(i).toString() == buildConfiguration) {
            m_buildConfigurationComboBox->setItemText(i, m_project->buildConfiguration(buildConfiguration)->displayName());
            break;
        }
    }
}


void BuildSettingsWidget::updateBuildSettings()
{
    // TODO save position, entry from combbox

    // Delete old tree items
    bool blocked = m_buildConfigurationComboBox->blockSignals(true);
    m_buildConfigurationComboBox->clear();
    m_subWidgets->clear();

    // update buttons
    m_removeButton->setEnabled(m_project->buildConfigurations().size() > 1);

    // Add pages
    BuildConfigWidget *generalConfigWidget = m_project->createConfigWidget();
    m_subWidgets->addWidget(generalConfigWidget->displayName(), generalConfigWidget);

    m_subWidgets->addWidget(tr("Build Steps"), new BuildStepsPage(m_project));
    m_subWidgets->addWidget(tr("Clean Steps"), new BuildStepsPage(m_project, true));

    QList<BuildConfigWidget *> subConfigWidgets = m_project->subConfigWidgets();
    foreach (BuildConfigWidget *subConfigWidget, subConfigWidgets)
        m_subWidgets->addWidget(subConfigWidget->displayName(), subConfigWidget);

    // Add tree items
    foreach (const BuildConfiguration *bc, m_project->buildConfigurations()) {
        m_buildConfigurationComboBox->addItem(bc->displayName(), bc->name());
        if (bc->name() == m_buildConfiguration)
            m_buildConfigurationComboBox->setCurrentIndex(m_buildConfigurationComboBox->count() - 1);
    }

    m_buildConfigurationComboBox->blockSignals(blocked);

    // TODO Restore position, entry from combbox
    // TODO? select entry from combobox ?
    activeBuildConfigurationChanged();
}

void BuildSettingsWidget::currentIndexChanged(int index)
{
    m_buildConfiguration = m_buildConfigurationComboBox->itemData(index).toString();
    activeBuildConfigurationChanged();
}

void BuildSettingsWidget::activeBuildConfigurationChanged()
{
    for (int i = 0; i < m_buildConfigurationComboBox->count(); ++i) {
        if (m_buildConfigurationComboBox->itemData(i).toString() == m_buildConfiguration) {
            m_buildConfigurationComboBox->setCurrentIndex(i);
            break;
        }
    }
    foreach (QWidget *widget, m_subWidgets->widgets()) {
        if (BuildConfigWidget *buildStepWidget = qobject_cast<BuildConfigWidget*>(widget)) {
            buildStepWidget->init(m_buildConfiguration);
        }
    }
    checkMakeActiveLabel();
}

void BuildSettingsWidget::checkMakeActiveLabel()
{
    m_makeActiveLabel->setVisible(false);
    if (!m_project->activeBuildConfiguration() || m_project->activeBuildConfiguration()->name() != m_buildConfiguration) {
        BuildConfiguration *bc = m_project->buildConfiguration(m_buildConfiguration);
        QTC_ASSERT(bc, return);
        m_makeActiveLabel->setText(tr("<a href=\"#\">Make %1 active.</a>").arg(bc->displayName()));
        m_makeActiveLabel->setVisible(true);
    }
}

void BuildSettingsWidget::createConfiguration()
{
    QAction *action = qobject_cast<QAction *>(sender());
    const QString &type = action->data().toString();
    if (m_project->buildConfigurationFactory()->create(type)) {
        // TODO switching to last buildconfiguration in list might not be what we want
        m_buildConfiguration = m_project->buildConfigurations().last()->name();
        updateBuildSettings();
    }
}

void BuildSettingsWidget::cloneConfiguration()
{
    const QString configuration = m_buildConfigurationComboBox->itemData(m_buildConfigurationComboBox->currentIndex()).toString();
    cloneConfiguration(configuration);
}

void BuildSettingsWidget::deleteConfiguration()
{
    const QString configuration = m_buildConfigurationComboBox->itemData(m_buildConfigurationComboBox->currentIndex()).toString();
    deleteConfiguration(configuration);
}

void BuildSettingsWidget::cloneConfiguration(const QString &sourceConfiguration)
{
    if (sourceConfiguration.isEmpty())
        return;

    QString newBuildConfiguration = QInputDialog::getText(this, tr("Clone configuration"), tr("New Configuration Name:"));
    if (newBuildConfiguration.isEmpty())
        return;

    QString newDisplayName = newBuildConfiguration;
    QStringList buildConfigurationDisplayNames;
    foreach(BuildConfiguration *bc, m_project->buildConfigurations())
        buildConfigurationDisplayNames << bc->displayName();
    newDisplayName = Project::makeUnique(newDisplayName, buildConfigurationDisplayNames);

    QStringList buildConfigurationNames;
    foreach(BuildConfiguration *bc, m_project->buildConfigurations())
        buildConfigurationNames << bc->name();

    newBuildConfiguration = Project::makeUnique(newBuildConfiguration, buildConfigurationNames);

    m_project->copyBuildConfiguration(sourceConfiguration, newBuildConfiguration);
    m_project->setDisplayNameFor(m_project->buildConfiguration(newBuildConfiguration), newDisplayName);

    m_buildConfiguration = newBuildConfiguration;
    updateBuildSettings();
}

void BuildSettingsWidget::deleteConfiguration(const QString &deleteConfiguration)
{
    if (deleteConfiguration.isEmpty() || m_project->buildConfigurations().size() <= 1)
        return;

    if (m_project->activeBuildConfiguration()->name() == deleteConfiguration) {
        foreach (BuildConfiguration *bc, m_project->buildConfigurations()) {
            if (bc->name() != deleteConfiguration) {
                m_project->setActiveBuildConfiguration(bc);
                break;
            }
        }
    }

    if (m_buildConfiguration == deleteConfiguration) {
        foreach (const BuildConfiguration *bc, m_project->buildConfigurations()) {
            if (bc->name() != deleteConfiguration) {
                m_buildConfiguration = bc->name();
                break;
            }
        }
    }

    m_project->removeBuildConfiguration(m_project->buildConfiguration(deleteConfiguration));

    updateBuildSettings();
}
