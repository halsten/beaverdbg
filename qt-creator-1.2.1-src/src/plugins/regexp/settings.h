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

#ifndef REGEXP_SETTINGS_H
#define REGEXP_SETTINGS_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QRegExp>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace RegExp {
namespace Internal {

// Settings of the Regexp plugin.
// Provides methods to serialize into QSettings.

struct Settings {
    Settings();
    void clear();
    void fromQSettings(QSettings *s);
    void toQSettings(QSettings *s) const;

    QRegExp::PatternSyntax m_patternSyntax;
    bool m_minimal;
    bool m_caseSensitive;

    QStringList m_patterns;
    QString m_currentPattern;
    QStringList m_matches;
    QString m_currentMatch;
};

} // namespace Internal
} // namespace RegExp

#endif // REGEXP_SETTINGS_H