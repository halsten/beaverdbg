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

#ifndef PASTEBINDOTCOMPROTOCOL_H
#define PASTEBINDOTCOMPROTOCOL_H

#include "protocol.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QHttp>

class PasteBinDotComSettings;

class PasteBinDotComProtocol : public Protocol
{
    Q_OBJECT
public:
    PasteBinDotComProtocol();

    QString name() const { return QLatin1String("Pastebin.Com"); }

    bool hasSettings() const { return true; }
    Core::IOptionsPage* settingsPage();

    bool canList() const { return false; }

    void fetch(const QString &id);
    void paste(const QString &text,
               const QString &username = QString(),
               const QString &comment = QString(),
               const QString &description = QString());
public slots:
    void fetchFinished();

    void postRequestFinished(int id, bool error);
    void readPostResponseHeader(const QHttpResponseHeader &);

private:
    PasteBinDotComSettings *settings;
    QNetworkAccessManager manager;
    QNetworkReply *reply;
    QString fetchId;

    QHttp http;
    int postId;
};

#endif // PASTEBINDOTCOMPROTOCOL_H
