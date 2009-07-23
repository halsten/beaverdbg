/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact:  Qt Software Information (qt-info@nokia.com)
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
** contact the sales department at qt-sales@nokia.com.
**
**************************************************************************/

#ifndef PERSISTENTSETTINGS_H
#define PERSISTENTSETTINGS_H

#include "projectexplorer_export.h"

#include <QtCore/QMap>
#include <QtCore/QVariant>
#include <QtXml/QDomElement>

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT PersistentSettingsReader
{
public:
    PersistentSettingsReader();
    QVariant restoreValue(const QString & variable) const;
    bool load(const QString & fileName);
    void setPrefix(const QString &prefix);
    QString prefix() const;
private:
    QString m_prefix;
    QVariant readValue(const QDomElement &valElement) const;
    void readValues(const QDomElement &data);
    QMap<QString, QVariant> m_valueMap;
};

class PROJECTEXPLORER_EXPORT PersistentSettingsWriter
{
public:
    PersistentSettingsWriter();
    void saveValue(const QString & variable, const QVariant &value);
    bool save(const QString & fileName, const QString & docType);
    void setPrefix(const QString &prefix);
    QString prefix() const;
private:
    QString m_prefix;
    void writeValue(QDomElement &ps, const QVariant &value);
    QMap<QString, QVariant> m_valueMap;
};

} // namespace ProjectExplorer

#endif // PERSISTENTSETTINGS_H
