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

#include "s60manager.h"

#include "s60devicespreferencepane.h"
#include "serialdevicelister.h"
#include "winscwtoolchain.h"
#include "gccetoolchain.h"
#include "rvcttoolchain.h"
#include "s60emulatorrunconfiguration.h"
#include "s60devicerunconfiguration.h"

#include <coreplugin/icore.h>
#include <extensionsystem/pluginmanager.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/toolchain.h>
#include <debugger/debuggermanager.h>
#include <utils/qtcassert.h>

#include <QtGui/QMainWindow>

namespace {
    const char *GCCE_COMMAND = "arm-none-symbianelf-gcc.exe";
    const char *S60_AUTODETECTION_SOURCE = "QTS60";
}

namespace Qt4ProjectManager {
namespace Internal {

S60Manager *S60Manager::m_instance = 0;

// ======== Parametrizable Factory for RunControls, depending on the configuration
// class and mode.

template <class RunControl, class RunConfiguration>
        class RunControlFactory : public ProjectExplorer::IRunControlFactory
{
public:
    explicit RunControlFactory(const QString &mode,
                               const QString &name,
                               QObject *parent = 0) :
    IRunControlFactory(parent), m_mode(mode), m_name(name) {}

    bool canRun(const QSharedPointer<ProjectExplorer::RunConfiguration> &runConfiguration, const QString &mode) const {
        return (mode == m_mode)
                && (!runConfiguration.objectCast<RunConfiguration>().isNull());
    }

    ProjectExplorer::RunControl* create(const QSharedPointer<ProjectExplorer::RunConfiguration> &runConfiguration, const QString &mode) {
        const QSharedPointer<RunConfiguration> rc = runConfiguration.objectCast<RunConfiguration>();
        QTC_ASSERT(!rc.isNull() && mode == m_mode, return 0);
        return new RunControl(rc);
    }

    QString displayName() const {
        return m_name;
    }

    QWidget *configurationWidget(const QSharedPointer<ProjectExplorer::RunConfiguration> & /*runConfiguration */) {
        return 0;
    }

private:
    const QString m_mode;
    const QString m_name;
};

// ======== S60Manager

S60Manager *S60Manager::instance() { return m_instance; }

S60Manager::S60Manager(QObject *parent)
        : QObject(parent),
        m_devices(new S60Devices(this)),
        m_serialDeviceLister(new SerialDeviceLister(this))
{
    m_instance = this;
 
    addAutoReleasedObject(new S60DevicesPreferencePane(m_devices, this));
    m_devices->detectQtForDevices(); // Order!
  
    addAutoReleasedObject(new S60EmulatorRunConfigurationFactory(this));
    addAutoReleasedObject(new RunControlFactory<S60EmulatorRunControl,
                                                S60EmulatorRunConfiguration>
                                                (QLatin1String(ProjectExplorer::Constants::RUNMODE),
                                                 tr("Run in Emulator"), parent));
    addAutoReleasedObject(new S60DeviceRunConfigurationFactory(this));
    addAutoReleasedObject(new RunControlFactory<S60DeviceRunControl,
                                                S60DeviceRunConfiguration>
                                                (QLatin1String(ProjectExplorer::Constants::RUNMODE),
                                                 tr("Run on Device"), parent));

    if (Debugger::DebuggerManager::instance())
        addAutoReleasedObject(new RunControlFactory<S60DeviceDebugRunControl,
                                                S60DeviceRunConfiguration>
                                                (QLatin1String(ProjectExplorer::Constants::DEBUGMODE),
                                                 tr("Debug on Device"), parent));
    updateQtVersions();
    connect(m_devices, SIGNAL(qtVersionsChanged()),
            this, SLOT(updateQtVersions()));
    connect(Core::ICore::instance()->mainWindow(), SIGNAL(deviceChange()),
            m_serialDeviceLister, SLOT(update()));
}

S60Manager::~S60Manager()
{
    ExtensionSystem::PluginManager *pm = ExtensionSystem::PluginManager::instance();
    for (int i = m_pluginObjects.size() - 1; i >= 0; i--) {
        pm->removeObject(m_pluginObjects.at(i));
        delete m_pluginObjects.at(i);
    }
}

void S60Manager::addAutoReleasedObject(QObject *o)
{
    ExtensionSystem::PluginManager::instance()->addObject(o);
    m_pluginObjects.push_back(o);
}

QString S60Manager::deviceIdFromDetectionSource(const QString &autoDetectionSource) const
{
    if (autoDetectionSource.startsWith(S60_AUTODETECTION_SOURCE))
        return autoDetectionSource.mid(QString(S60_AUTODETECTION_SOURCE).length()+1);
    return QString();
}

void S60Manager::updateQtVersions()
{
    // This assumes that the QtVersionManager has already read
    // the Qt versions from the settings
    QtVersionManager *versionManager = QtVersionManager::instance();
    QList<QtVersion *> versions = versionManager->versions();
    QList<QtVersion *> handledVersions;
    QList<QtVersion *> versionsToAdd;
    foreach (const S60Devices::Device &device, m_devices->devices()) {
        if (device.qt.isEmpty()) // no Qt version found for this sdk
            continue;
        QtVersion *deviceVersion = 0;
        // look if we have a respective Qt version already
        foreach (QtVersion *version, versions) {
            if (version->isAutodetected()
                    && deviceIdFromDetectionSource(version->autodetectionSource()) == device.id) {
                deviceVersion = version;
                break;
            }
        }
        if (deviceVersion) {
            deviceVersion->setQMakeCommand(device.qt+"/bin/qmake.exe");
            deviceVersion->setName(QString("%1 (Qt %2)").arg(device.id, deviceVersion->qtVersionString()));
            handledVersions.append(deviceVersion);
        } else {
            deviceVersion = new QtVersion(QString("%1 (Qt %2)").arg(device.id), device.qt+"/bin/qmake.exe",
                                          true, QString("%1.%2").arg(S60_AUTODETECTION_SOURCE, device.id));
            deviceVersion->setName(deviceVersion->name().arg(deviceVersion->qtVersionString()));
            versionsToAdd.append(deviceVersion);
        }
        deviceVersion->setS60SDKDirectory(device.epocRoot);
    }
    // remove old autodetected versions
    foreach (QtVersion *version, versions) {
        if (version->isAutodetected()
                && version->autodetectionSource().startsWith(S60_AUTODETECTION_SOURCE)
                && !handledVersions.contains(version)) {
            versionManager->removeVersion(version);
        }
    }
    // add new versions
    foreach (QtVersion *version, versionsToAdd) {
        versionManager->addVersion(version);
    }
}

ProjectExplorer::ToolChain *S60Manager::createWINSCWToolChain(const Qt4ProjectManager::QtVersion *version) const
{
    return new WINSCWToolChain(deviceForQtVersion(version), version->mwcDirectory());
}

ProjectExplorer::ToolChain *S60Manager::createGCCEToolChain(const Qt4ProjectManager::QtVersion *version) const
{
    ProjectExplorer::Environment env = ProjectExplorer::Environment::systemEnvironment();
    env.prependOrSetPath(version->gcceDirectory()+"/bin");
    QString gcceCommandPath= env.searchInPath(GCCE_COMMAND);
    return new GCCEToolChain(deviceForQtVersion(version), gcceCommandPath);
}

ProjectExplorer::ToolChain *S60Manager::createRVCTToolChain(
        const Qt4ProjectManager::QtVersion *version,
        ProjectExplorer::ToolChain::ToolChainType type) const
{
    return new RVCTToolChain(deviceForQtVersion(version), type);
}

S60Devices::Device S60Manager::deviceForQtVersion(const Qt4ProjectManager::QtVersion *version) const
{
    S60Devices::Device device;
    QString deviceId;
    if (version->isAutodetected())
        deviceId = deviceIdFromDetectionSource(version->autodetectionSource());
    if (deviceId.isEmpty()) { // it's not an s60 autodetected version
        // try to find a device entry belonging to the root given in Qt prefs
        QString sdkRoot = version->s60SDKDirectory();
        device = m_devices->deviceForEpocRoot(sdkRoot);
        if (device.epocRoot.isEmpty()) { // no device found
            // check if we can construct a dummy one
            if (QFile::exists(QString::fromLatin1("%1/epoc32").arg(sdkRoot))) {
                device.epocRoot = sdkRoot;
                device.toolsRoot = device.epocRoot;
                device.qt = QFileInfo(QFileInfo(version->qmakeCommand()).path()).path();
                device.isDefault = false;
                device.name = QString::fromLatin1("Manual");
                device.id = QString::fromLatin1("Manual");
            }
        }
    } else {
        device = m_devices->deviceForId(deviceId);
    }
    return device;
}

} // namespace internal
} // namespace qt4projectmanager
