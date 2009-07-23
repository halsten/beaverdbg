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

#include "annotationhighlighter.h"

using namespace Subversion;
using namespace Subversion::Internal;

SubversionAnnotationHighlighter::SubversionAnnotationHighlighter(const ChangeNumbers &changeNumbers,
                                                             QTextDocument *document) :
    VCSBase::BaseAnnotationHighlighter(changeNumbers, document),
    m_blank(QLatin1Char(' '))
{
}

QString SubversionAnnotationHighlighter::changeNumber(const QString &block) const
{
    const int pos = block.indexOf(m_blank);
    return pos > 1 ? block.left(pos) : QString();
}
