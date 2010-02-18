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

#ifndef CHECKOUTWIZARD_H
#define CHECKOUTWIZARD_H

#include <vcsbase/basecheckoutwizard.h>

namespace Subversion {
namespace Internal {

class CheckoutWizard : public VCSBase::BaseCheckoutWizard
{
    Q_OBJECT
public:
    explicit CheckoutWizard(QObject *parent = 0);

    // IWizard
    virtual QIcon icon() const;
    virtual QString description() const;
    virtual QString name() const;

protected:
    // BaseCheckoutWizard
    virtual QList<QWizardPage*> createParameterPages(const QString &path);
    virtual QSharedPointer<VCSBase::AbstractCheckoutJob> createJob(const QList<QWizardPage*> &parameterPage,
                                                                   QString *checkoutPath);
};

} // namespace Internal
} // namespace Subversion

#endif // CHECKOUTWIZARD_H
