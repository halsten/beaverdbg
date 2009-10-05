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

#ifndef BUILDCONFIGURATION_H
#define BUILDCONFIGURATION_H

#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QHash>

namespace ProjectExplorer {
namespace Internal {

class BuildConfiguration
{
public:
    BuildConfiguration(const QString &name);
    BuildConfiguration(const QString &name, BuildConfiguration *source);
    QString name() const;
    QVariant getValue(const QString &key) const;
    void setValue(const QString &key, QVariant value);

    QString displayName();
    void setDisplayName(const QString &name);

    QMap<QString, QVariant> toMap() const;
    void setValuesFromMap(QMap<QString, QVariant> map);

private:
    QHash<QString, QVariant> m_values;
    QString m_name;
};

}
} // namespace ProjectExplorer

#endif // BUILDCONFIGURATION_H
