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

#include "idcollector.h"
#include "qmljsast_p.h"
#include "qmljsengine_p.h"

using namespace QmlJS;
using namespace QmlJS::AST;
using namespace QmlEditor;
using namespace QmlEditor::Internal;

QMap<QString, QmlIdSymbol*> IdCollector::operator()(const QString &fileName, QmlJS::AST::UiProgram *ast)
{
    _fileName = fileName;
    _ids.clear();

    Node::accept(ast, this);

    return _ids;
}

bool IdCollector::visit(QmlJS::AST::UiObjectBinding *ast)
{
    _scopes.push(ast);
    return true;
}

bool IdCollector::visit(QmlJS::AST::UiObjectDefinition *ast)
{
    _scopes.push(ast);
    return true;
}

void IdCollector::endVisit(QmlJS::AST::UiObjectBinding *)
{
    _scopes.pop();
}

void IdCollector::endVisit(QmlJS::AST::UiObjectDefinition *)
{
    _scopes.pop();
}

bool IdCollector::visit(QmlJS::AST::UiScriptBinding *ast)
{
    if (!(ast->qualifiedId->next) && ast->qualifiedId->name->asString() == "id")
        if (ExpressionStatement *e = cast<ExpressionStatement*>(ast->statement))
            if (IdentifierExpression *i = cast<IdentifierExpression*>(e->expression))
                addId(i->name->asString(), ast);

    return false;
}

void IdCollector::addId(const QString &id, QmlJS::AST::UiScriptBinding *ast)
{
    if (!_ids.contains(id)) {
        Node *parent = _scopes.top();

        if (UiObjectBinding *binding = cast<UiObjectBinding*>(parent))
            _ids[id] = new QmlIdSymbol(_fileName, ast, QmlSymbolFromFile(_fileName, binding));
        else if (UiObjectDefinition *definition = cast<UiObjectDefinition*>(parent))
            _ids[id] = new QmlIdSymbol(_fileName, ast, QmlSymbolFromFile(_fileName, definition));
        else
            Q_ASSERT(!"Unknown parent for id");
    }
}
