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

#ifndef TEXTFILEWIZARD_H
#define TEXTFILEWIZARD_H

#include "texteditor_global.h"

#include <coreplugin/basefilewizard.h>

namespace TextEditor {

class TEXTEDITOR_EXPORT TextFileWizard : public Core::StandardFileWizard
{
    Q_OBJECT

public:
    typedef Core::BaseFileWizardParameters BaseFileWizardParameters;
    TextFileWizard(const QString &mimeType,
                   const QString &editorKind,
                   const QString &suggestedFileName,
                   const BaseFileWizardParameters &parameters,
                   QObject *parent = 0);

protected:
    virtual Core::GeneratedFiles
        generateFilesFromPath(const QString &path, const QString &name,
                              QString *errorMessage) const;
private:
    const QString m_mimeType;
    const QString m_editorKind;
    const QString m_suggestedFileName;
};

} // namespace TextEditor

#endif // TEXTFILEWIZARD_H
