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

#include "emptyprojectwizard.h"

#include "emptyprojectwizarddialog.h"

#include <utils/pathchooser.h>

namespace Qt4ProjectManager {
namespace Internal {

EmptyProjectWizard::EmptyProjectWizard()
  : QtWizard(tr("Empty Qt4 Project"),
             tr("Creates an empty Qt project."),
             QIcon(":/wizards/images/gui.png"))
{
}

QWizard *EmptyProjectWizard::createWizardDialog(QWidget *parent,
                                              const QString &defaultPath,
                                              const WizardPageList &extensionPages) const
{
    EmptyProjectWizardDialog *dialog = new EmptyProjectWizardDialog(name(), icon(), extensionPages, parent);
    dialog->setPath(defaultPath.isEmpty() ? Core::Utils::PathChooser::homePath() : defaultPath);
    return dialog;
}

Core::GeneratedFiles
        EmptyProjectWizard::generateFiles(const QWizard *w,
                                        QString * /*errorMessage*/) const
{
    const EmptyProjectWizardDialog *wizard = qobject_cast< const EmptyProjectWizardDialog *>(w);
    const QtProjectParameters params = wizard->parameters();
    const QString projectPath = params.projectPath();
    const QString profileName = Core::BaseFileWizard::buildFileName(projectPath, params.name, profileSuffix());

    Core::GeneratedFile profile(profileName);
    return Core::GeneratedFiles() << profile;
}

} // namespace Internal
} // namespace Qt4ProjectManager
