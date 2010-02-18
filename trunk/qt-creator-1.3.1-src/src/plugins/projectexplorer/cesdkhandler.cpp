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

#include "cesdkhandler.h"
#include "environment.h"

#include <QtCore/QFile>
#include <QtCore/QDebug>
#include <QtCore/QXmlStreamReader>

using namespace ProjectExplorer;
using namespace ProjectExplorer::Internal;
using ProjectExplorer::Environment;

CeSdkInfo::CeSdkInfo()
    : m_major(0), m_minor(0)
{
}

void CeSdkInfo::addToEnvironment(Environment &env)
{
    qDebug() << "adding " << name() << "to Environment";
    env.set("INCLUDE", m_include);
    env.set("LIB", m_lib);
    env.prependOrSetPath(m_bin);
    qDebug()<<env.toStringList();
}

CeSdkHandler::CeSdkHandler()
{
}

static void readMkSpec(const QString &qtpath, QString *ceSdk, QString *ceArch)
{
    QFile f(qtpath);
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        while (!f.atEnd()) {
            QByteArray line = f.readLine();
            if (line.startsWith("CE_SDK")) {
                int index = line.indexOf('=');
                if (index >= 0) {
                    *ceSdk = line.mid(index + 1).trimmed();
                }
            } else if (line.startsWith("CE_ARCH")) {
                int index = line.indexOf('=');
                if (index >= 0) {
                    *ceArch = line.mid(index + 1).trimmed();
                }
            } else if (line.startsWith("include(")) {
                int startIndex = line.indexOf('(');
                int endIndex = line.indexOf(')');
                if (startIndex >= 0 && endIndex >= 0) {
                    QString path = line.mid(startIndex + 1, endIndex - startIndex - 1).trimmed();

                    int index = qtpath.lastIndexOf('/');
                    if (index >= 0)
                        readMkSpec(qtpath.left(index + 1) + path, ceSdk, ceArch);
                    else
                        readMkSpec(path, ceSdk, ceArch);
                }
            }
        }
    }
}

QString CeSdkHandler::platformName(const QString &qtpath)
{
    QString platformName;
    QString CE_SDK;
    QString CE_ARCH;

    readMkSpec(qtpath, &CE_SDK, &CE_ARCH);

    if (!CE_SDK.isEmpty() && !CE_ARCH.isEmpty())
        platformName = CE_SDK + " (" + CE_ARCH + ")";

    return platformName;
}

bool CeSdkHandler::parse(const QString &vsdir)
{
    // look at the file at %VCInstallDir%/vcpackages/WCE.VCPlatform.config
    // and scan through all installed sdks...
    m_list.clear();

    VCInstallDir = vsdir;

    QDir vStudioDir(VCInstallDir);
    if (!vStudioDir.cd("vcpackages"))
        return false;

    QFile configFile(vStudioDir.absoluteFilePath(QLatin1String("WCE.VCPlatform.config")));
    qDebug()<<"##";
    if (!configFile.exists() || !configFile.open(QIODevice::ReadOnly))
        return false;

    qDebug()<<"parsing";

    QString currentElement;
    CeSdkInfo currentItem;
    QXmlStreamReader xml(&configFile);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            currentElement = xml.name().toString();
            if (currentElement == QLatin1String("Platform"))
                currentItem = CeSdkInfo();
            else if (currentElement == QLatin1String("Directories")) {
                QXmlStreamAttributes attr = xml.attributes();
                currentItem.m_include = fixPaths(attr.value(QLatin1String("Include")).toString());
                currentItem.m_lib = fixPaths(attr.value(QLatin1String("Library")).toString());
                currentItem.m_bin = fixPaths(attr.value(QLatin1String("Path")).toString());
            }
        } else if (xml.isEndElement()) {
            if (xml.name().toString() == QLatin1String("Platform"))
                m_list.append(currentItem);
        } else if (xml.isCharacters() && !xml.isWhitespace()) {
            if (currentElement == QLatin1String("PlatformName"))
                currentItem.m_name = xml.text().toString();
            else if (currentElement == QLatin1String("OSMajorVersion"))
                currentItem.m_major = xml.text().toString().toInt();
            else if (currentElement == QLatin1String("OSMinorVersion"))
                currentItem.m_minor = xml.text().toString().toInt();
        }
    }

    if (xml.error() && xml.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
        qWarning() << "XML ERROR:" << xml.lineNumber() << ": " << xml.errorString();
        return false;
    }
    return m_list.size() > 0 ? true : false;
}

CeSdkInfo CeSdkHandler::find(const QString &name)
{
    qDebug() << "looking for platform " << name;
    for (QList<CeSdkInfo>::iterator it = m_list.begin(); it != m_list.end(); ++it) {
        qDebug() << "...." << it->name();
        if (it->name() == name)
            return *it;
    }
    return CeSdkInfo();
}
