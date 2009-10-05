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

#ifndef QT4PROJECTCONFIGWIDGET_H
#define QT4PROJECTCONFIGWIDGET_H

#include <projectexplorer/buildstep.h>

namespace Qt4ProjectManager {

class Qt4Project;

namespace Internal {

namespace Ui {
class Qt4ProjectConfigWidget;
}

class Qt4ProjectConfigWidget : public ProjectExplorer::BuildStepConfigWidget
{
    Q_OBJECT
public:
    Qt4ProjectConfigWidget(Qt4Project *project);
    ~Qt4ProjectConfigWidget();

    QString displayName() const;
    void init(const QString &buildConfiguration);

private slots:
    void changeConfigName(const QString &newName);
    void setupQtVersionsComboBox();
    void shadowBuildCheckBoxClicked(bool checked);
    void onBeforeBeforeShadowBuildDirBrowsed();
    void shadowBuildLineEditTextChanged();
    void importLabelClicked();
    void qtVersionComboBoxCurrentIndexChanged(const QString &);
    void manageQtVersions();

private:
    void updateImportLabel();
    Ui::Qt4ProjectConfigWidget *m_ui;
    Qt4Project *m_pro;
    QString m_buildConfiguration;
};

} // namespace Internal
} // namespace Qt4ProjectManager

#endif // QT4PROJECTCONFIGWIDGET_H
