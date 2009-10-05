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

#ifndef FORMEDITORPLUGIN_H
#define FORMEDITORPLUGIN_H

#include <extensionsystem/iplugin.h>

namespace Core {
    class IWizard;
    class ICore;
}

namespace Designer {
namespace Internal {

class FormEditorFactory;
class FormWizard;
class FormEditorW;

class FormEditorPlugin : public ExtensionSystem::IPlugin
{
    Q_OBJECT

public:
    FormEditorPlugin();
    ~FormEditorPlugin();

    //Plugin
    bool initialize(const QStringList &arguments, QString *error_message = 0);
    void extensionsInitialized();

private:
    bool initializeTemplates(QString *error_message);

    FormEditorFactory *m_factory;

    Core::IWizard *m_formWizard;
    Core::IWizard *m_formClassWizard;
};

} // namespace Internal
} // namespace Designer

#endif // FORMEDITORPLUGIN_H
