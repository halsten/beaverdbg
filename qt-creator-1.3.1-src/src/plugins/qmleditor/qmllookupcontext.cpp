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

#include "qmljsast_p.h"
#include "qmljsengine_p.h"

#include "qmlexpressionundercursor.h"
#include "qmllookupcontext.h"
#include "qmlresolveexpression.h"

using namespace QmlEditor;
using namespace QmlEditor::Internal;
using namespace QmlJS;
using namespace QmlJS::AST;

QmlLookupContext::QmlLookupContext(const QStack<QmlJS::AST::Node *> &scopes,
                                   const QmlDocument::Ptr &doc,
                                   const Snapshot &snapshot):
        _scopes(scopes),
        _doc(doc),
        _snapshot(snapshot)
{
}

QmlLookupContext::~QmlLookupContext()
{
    qDeleteAll(_temporarySymbols);
}

QmlSymbol *QmlLookupContext::resolve(const QString &name)
{
    // look at property definitions
    if (QmlSymbol *propertySymbol = resolveProperty(name, _scopes.top(), _doc->fileName()))
        return propertySymbol;

    if (name == "parent") {
        for (int i = _scopes.size() - 2; i >= 0; --i) {
            Node *scope = _scopes.at(i);

            if (UiObjectDefinition *definition = cast<UiObjectDefinition*>(scope))
                return createSymbol(_doc->fileName(), definition);
            else if (UiObjectBinding *binding = cast<UiObjectBinding*>(scope))
                return createSymbol(_doc->fileName(), binding);
        }

        return 0;
    }

    // look at the ids.
    const QmlDocument::IdTable ids = _doc->ids();

    if (ids.contains(name))
        return ids[name];
    else
        return 0;
}

QmlSymbol *QmlLookupContext::createSymbol(const QString &fileName, QmlJS::AST::UiObjectMember *node)
{
    QmlSymbol *symbol = new QmlSymbolFromFile(fileName, node);
    _temporarySymbols.append(symbol);
    return symbol;
}

QmlSymbol *QmlLookupContext::resolveType(const QString &name, const QString &fileName)
{
    // TODO: handle import-as.
    QmlDocument::Ptr document = _snapshot[fileName];
    if (document.isNull())
        return 0;

    UiProgram *prog = document->program();
    if (!prog)
        return 0;

    UiImportList *imports = prog->imports;
    if (!imports)
        return 0;

    for (UiImportList *iter = imports; iter; iter = iter->next) {
        UiImport *import = iter->import;
        if (!import)
            continue;

        if (!(import->fileName))
            continue;

        const QString path = import->fileName->asString();

        const QMap<QString, QmlDocument::Ptr> importedTypes = _snapshot.componentsDefinedByImportedDocuments(document, path);
        if (importedTypes.contains(name)) {
            QmlDocument::Ptr importedDoc = importedTypes.value(name);

            UiProgram *importedProgram = importedDoc->program();
            if (importedProgram && importedProgram->members && importedProgram->members->member)
                return createSymbol(importedDoc->fileName(), importedProgram->members->member);
        }
    }

    return 0;
}

QmlSymbol *QmlLookupContext::resolveProperty(const QString &name, Node *scope, const QString &fileName)
{
    UiQualifiedId *typeName = 0;

    if (UiObjectBinding *binding = cast<UiObjectBinding*>(scope)) {
        if (QmlSymbol *symbol = resolveProperty(name, binding->initializer, fileName))
            return symbol;
        else
            typeName = binding->qualifiedTypeNameId;
    } else if (UiObjectDefinition *definition = cast<UiObjectDefinition*>(scope)) {
        if (QmlSymbol *symbol = resolveProperty(name, definition->initializer, fileName))
            return symbol;
        else
            typeName = definition->qualifiedTypeNameId;
    } // TODO: extend this to handle (JavaScript) block scopes.

    if (typeName == 0)
        return 0;

    QmlSymbol *typeSymbol = resolveType(toString(typeName), fileName);
    if (typeSymbol && typeSymbol->isSymbolFromFile()) {
        return resolveProperty(name, typeSymbol->asSymbolFromFile()->node(), typeSymbol->asSymbolFromFile()->fileName());
    }

    return 0;
}

QmlSymbol *QmlLookupContext::resolveProperty(const QString &name, QmlJS::AST::UiObjectInitializer *initializer, const QString &fileName)
{
    if (!initializer)
        return 0;

    for (UiObjectMemberList *iter = initializer->members; iter; iter = iter->next) {
        UiObjectMember *member = iter->member;
        if (!member)
            continue;

        if (UiPublicMember *publicMember = cast<UiPublicMember*>(member)) {
            if (name == publicMember->name->asString())
                return createSymbol(fileName, publicMember);
        } else if (UiObjectBinding *objectBinding = cast<UiObjectBinding*>(member)) {
            if (name == toString(objectBinding->qualifiedId))
                return createSymbol(fileName, objectBinding);
        } else if (UiArrayBinding *arrayBinding = cast<UiArrayBinding*>(member)) {
            if (name == toString(arrayBinding->qualifiedId))
                return createSymbol(fileName, arrayBinding);
        } else if (UiScriptBinding *scriptBinding = cast<UiScriptBinding*>(member)) {
            if (name == toString(scriptBinding->qualifiedId))
                return createSymbol(fileName, scriptBinding);
        }
    }

    return 0;
}

QString QmlLookupContext::toString(UiQualifiedId *id)
{
    QString str;

    for (UiQualifiedId *iter = id; iter; iter = iter->next) {
        if (!(iter->name))
            continue;

        str.append(iter->name->asString());

        if (iter->next)
            str.append('.');
    }

    return str;
}

QList<QmlSymbol*> QmlLookupContext::visibleSymbols(QmlJS::AST::Node * /* scope */)
{
    // FIXME
    return QList<QmlSymbol*>();
}

QList<QmlSymbol*> QmlLookupContext::visibleTypes()
{
    QList<QmlSymbol*> result;

    UiProgram *program = _doc->program();
    if (!program)
        return result;

    for (UiImportList *iter = program->imports; iter; iter = iter->next) {
        UiImport *import = iter->import;
        if (!import)
            continue;

        if (!(import->fileName))
            continue;
        const QString path = import->fileName->asString();

        // TODO: handle "import as".

        const QMap<QString, QmlDocument::Ptr> types = _snapshot.componentsDefinedByImportedDocuments(_doc, path);
        foreach (const QmlDocument::Ptr typeDoc, types) {
            UiProgram *typeProgram = typeDoc->program();

            if (typeProgram && typeProgram->members && typeProgram->members->member) {
                result.append(createSymbol(typeDoc->fileName(), typeProgram->members->member));
            }
        }
    }

    return result;
}
