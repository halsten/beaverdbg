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

#ifndef JAVASCRIPTENGINE_P_H
#define JAVASCRIPTENGINE_P_H

#include "javascriptvalue.h"
#include <QString>
#include <QSet>

QT_BEGIN_NAMESPACE

namespace JavaScript {

class Node;
class Lexer;
class NodePool;

namespace AST {

class Node;

} // end of namespace AST

namespace Ecma {

class RegExp
{
public:
    enum RegExpFlag {
        Global     = 0x01,
        IgnoreCase = 0x02,
        Multiline  = 0x04
    };

public:
  static int flagFromChar(const QChar &);
};

} // end of namespace Ecma

} // end of namespace JavaScript



class JavaScriptNameIdImpl
{
    QString _text;

public:
    JavaScriptNameIdImpl(const QChar *u, int s)
        : _text(u, s)
    { }

    const QString asString() const
    { return _text; }

    bool operator == (const JavaScriptNameIdImpl &other) const
    { return _text == other._text; }

    bool operator != (const JavaScriptNameIdImpl &other) const
    { return _text != other._text; }

    bool operator < (const JavaScriptNameIdImpl &other) const
    { return _text < other._text; }
};

inline uint qHash(const JavaScriptNameIdImpl &id)
{ return qHash(id.asString()); }

class JavaScriptEnginePrivate
{
    JavaScript::Lexer *_lexer;
    JavaScript::NodePool *_nodePool;
    JavaScript::AST::Node *_ast;
    QSet<JavaScriptNameIdImpl> _literals;

public:
    JavaScriptEnginePrivate()
        : _lexer(0), _nodePool(0), _ast(0)
    { }

    QSet<JavaScriptNameIdImpl> literals() const
    { return _literals; }

    JavaScriptNameIdImpl *intern(const QChar *u, int s)
    { return const_cast<JavaScriptNameIdImpl *>(&*_literals.insert(JavaScriptNameIdImpl(u, s))); }

    JavaScript::Lexer *lexer() const
    { return _lexer; }

    void setLexer(JavaScript::Lexer *lexer)
    { _lexer = lexer; }

    JavaScript::NodePool *nodePool() const
    { return _nodePool; }

    void setNodePool(JavaScript::NodePool *nodePool)
    { _nodePool = nodePool; }

    JavaScript::AST::Node *ast() const
    { return _ast; }

    JavaScript::AST::Node *changeAbstractSyntaxTree(JavaScript::AST::Node *node)
    {
        JavaScript::AST::Node *previousAST = _ast;
        _ast = node;
        return previousAST;
    }
};

QT_END_NAMESPACE

#endif // JAVASCRIPTENGINE_P_H
