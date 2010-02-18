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

#include "gitoriousclonewizard.h"
#include "gitorioushostwizardpage.h"
#include "gitoriousprojectwizardpage.h"
#include "gitoriousrepositorywizardpage.h"
#include "clonewizardpage.h"

#include <vcsbase/checkoutjobs.h>
#include <utils/qtcassert.h>

#include <QtCore/QUrl>
#include <QtGui/QIcon>

namespace Gitorious {
namespace Internal {

//  GitoriousCloneWizardPage: A git clone page taking its URL from the
//  projects page.

class GitoriousCloneWizardPage : public Git::CloneWizardPage {
public:
    explicit GitoriousCloneWizardPage(const GitoriousRepositoryWizardPage *rp, QWidget *parent = 0);
    virtual void initializePage();

private:
    const GitoriousRepositoryWizardPage *m_repositoryPage;
};

GitoriousCloneWizardPage::GitoriousCloneWizardPage(const GitoriousRepositoryWizardPage *rp, QWidget *parent) :
    Git::CloneWizardPage(parent),
    m_repositoryPage(rp)
{
}

void GitoriousCloneWizardPage::initializePage()
{
    setRepository(m_repositoryPage->repositoryURL().toString());
}

// -------- GitoriousCloneWizard
GitoriousCloneWizard::GitoriousCloneWizard(QObject *parent) :
        VCSBase::BaseCheckoutWizard(parent)
{
}

QIcon GitoriousCloneWizard::icon() const
{
    return QIcon();
}

QString GitoriousCloneWizard::description() const
{
    return tr("Clones a project from a Gitorious repository.");
}

QString GitoriousCloneWizard::name() const
{
    return tr("Gitorious Repository Clone");
}

QList<QWizardPage*> GitoriousCloneWizard::createParameterPages(const QString &path)
{
    GitoriousHostWizardPage *hostPage = new GitoriousHostWizardPage;
    GitoriousProjectWizardPage *projectPage = new GitoriousProjectWizardPage(hostPage);
    GitoriousRepositoryWizardPage *repoPage = new GitoriousRepositoryWizardPage(projectPage);
    GitoriousCloneWizardPage *clonePage = new GitoriousCloneWizardPage(repoPage);
    clonePage->setPath(path);

    QList<QWizardPage*> rc;
    rc << hostPage << projectPage << repoPage << clonePage;
    return rc;
}

QSharedPointer<VCSBase::AbstractCheckoutJob> GitoriousCloneWizard::createJob(const QList<QWizardPage*> &parameterPages,
                                                                    QString *checkoutPath)
{
    const Git::CloneWizardPage *cwp = qobject_cast<const Git::CloneWizardPage *>(parameterPages.back());
    QTC_ASSERT(cwp, return QSharedPointer<VCSBase::AbstractCheckoutJob>())
    return cwp->createCheckoutJob(checkoutPath);
}

} // namespace Internal
} // namespace Gitorius
