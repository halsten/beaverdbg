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

#include "s60devicespreferencepane.h"
#include "ui_s60devicespreferencepane.h"

#include <qt4projectmanager/qt4projectmanagerconstants.h>

#include <QtDebug>

using namespace Qt4ProjectManager;
using namespace Qt4ProjectManager::Internal;

S60DevicesWidget::S60DevicesWidget(QWidget *parent, S60Devices *devices) :
    QWidget(parent),
    m_ui(new Ui::S60DevicesPreferencePane),
    m_devices(devices)
{
    m_ui->setupUi(this);
    connect(m_ui->refreshButton, SIGNAL(clicked()), this, SLOT(updateDevices()));
    updateDevicesList();
}

S60DevicesWidget::~S60DevicesWidget()
{
    delete m_ui;
}

void S60DevicesWidget::updateDevices()
{
    m_devices->read();
    Q_ASSERT(m_devices->detectQtForDevices());
    updateDevicesList();
}

void S60DevicesWidget::updateDevicesList()
{
    QList<S60Devices::Device> devices = m_devices->devices();
    m_ui->list->clear();
    foreach (const S60Devices::Device &device, devices) {
        QStringList columns;
        columns << device.epocRoot << (device.qt.isEmpty()?tr("No Qt installed"):device.qt);
        QTreeWidgetItem *item = new QTreeWidgetItem(columns);
        const QString tooltip = device.toHtml();
        item->setToolTip(0, tooltip);
        item->setToolTip(1, tooltip);
        m_ui->list->addTopLevelItem(item);
    }
    const QString errorString = m_devices->errorString();
    if (errorString.isEmpty()) {
        clearErrorLabel();
    } else {
        setErrorLabel(errorString);
    }
}

void S60DevicesWidget::setErrorLabel(const QString& t)
{
    m_ui->errorLabel->setText(t);
    m_ui->errorLabel->setVisible(true);
}

void S60DevicesWidget::clearErrorLabel()
{
    m_ui->errorLabel->setVisible(false);
}


S60DevicesPreferencePane::S60DevicesPreferencePane(S60Devices *devices, QObject *parent)
        : Core::IOptionsPage(parent),
        m_widget(0),
        m_devices(devices)
{
    m_devices->read();
}

S60DevicesPreferencePane::~S60DevicesPreferencePane()
{
}

QString S60DevicesPreferencePane::id() const
{
    return QLatin1String("S60 SDKs");
}

QString S60DevicesPreferencePane::trName() const
{
    return tr("S60 SDKs");
}

QString S60DevicesPreferencePane::category() const
{
    return Constants::QT_CATEGORY;
}

QString S60DevicesPreferencePane::trCategory() const
{
    return tr(Constants::QT_CATEGORY);
}

QWidget *S60DevicesPreferencePane::createPage(QWidget *parent)
{
    if (m_widget)
        delete m_widget;
    m_widget = new S60DevicesWidget(parent, m_devices);
    return m_widget;
}

void S60DevicesPreferencePane::apply()
{
}

void S60DevicesPreferencePane::finish()
{
}


