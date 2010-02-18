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

#ifndef CURRENTPROJECTFILTER_H
#define CURRENTPROJECTFILTER_H

#include <locator/basefilefilter.h>

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QFutureInterface>
#include <QtCore/QTimer>
#include <QtGui/QWidget>

namespace ProjectExplorer {

class ProjectExplorerPlugin;
class Project;

namespace Internal {

class CurrentProjectFilter : public Locator::BaseFileFilter
{
    Q_OBJECT

public:
    CurrentProjectFilter(ProjectExplorerPlugin *pe);
    QString trName() const { return tr("Files in current project"); }
    QString name() const { return "Files in current project"; }
    Locator::ILocatorFilter::Priority priority() const { return Locator::ILocatorFilter::Low; }
    void refresh(QFutureInterface<void> &future);

protected:
    void updateFiles();

private slots:
    void currentProjectChanged(ProjectExplorer::Project *project);
    void markFilesAsOutOfDate();

private:

    ProjectExplorerPlugin *m_projectExplorer;
    Project *m_project;
    bool m_filesUpToDate;
};

} // namespace Internal
} // namespace ProjectExplorer

#endif // CURRENTPROJECTFILTER_H
