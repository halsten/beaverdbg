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

#ifndef CUSTOMWIDGETWIZARDDIALOG_H
#define CUSTOMWIDGETWIZARDDIALOG_H

#include <QtCore/QSharedPointer>

#include <QtGui/QWizard>
#include <QtGui/QWizardPage>

namespace Utils {
    class ProjectIntroPage;
}

namespace Qt4ProjectManager {
namespace Internal {

class CustomWidgetWidgetsWizardPage;
class CustomWidgetPluginWizardPage;
struct PluginOptions;
struct FileNamingParameters;

class CustomWidgetWizardDialog : public QWizard
{
    Q_OBJECT
public:
    explicit CustomWidgetWizardDialog(const QString &templateName,
                                      const QIcon &icon,
                                      const QList<QWizardPage*> &extensionPages,
                                      QWidget *parent);

    QSharedPointer<PluginOptions> pluginOptions() const;

    QString path() const;
    QString name() const;

    FileNamingParameters fileNamingParameters() const;
    void setFileNamingParameters(const FileNamingParameters &fnp);

public slots:
    void setPath(const QString &path);

private slots:
    void slotCurrentIdChanged (int id);

private:
    Utils::ProjectIntroPage *m_introPage;
    CustomWidgetWidgetsWizardPage *m_widgetsPage;
    CustomWidgetPluginWizardPage *m_pluginPage;
};

} // namespace Internal
} // namespace Qt4ProjectManager

#endif // CUSTOMWIDGETWIZARDDIALOG_H
