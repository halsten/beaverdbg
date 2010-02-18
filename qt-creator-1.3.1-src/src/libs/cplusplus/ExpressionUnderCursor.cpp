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

#include "ExpressionUnderCursor.h"
#include "SimpleLexer.h"
#include "BackwardsScanner.h"
#include <Token.h>

#include <QTextCursor>
#include <QTextBlock>

using namespace CPlusPlus;

ExpressionUnderCursor::ExpressionUnderCursor()
    : _jumpedComma(false)
{ }

ExpressionUnderCursor::~ExpressionUnderCursor()
{ }

int ExpressionUnderCursor::startOfExpression(BackwardsScanner &tk, int index)
{
    index = startOfExpression_helper(tk, index);

    if (_jumpedComma) {
        const SimpleToken &tok = tk[index - 1];

        switch (tok.kind()) {
        case T_COMMA:
        case T_LPAREN:
        case T_LBRACKET:
        case T_LBRACE:
        case T_SEMICOLON:
        case T_COLON:
        case T_QUESTION:
            break;

        default:
            if (tok.isOperator())
                return startOfExpression(tk, index - 1);

            break;
        }
    }

    return index;
}

int ExpressionUnderCursor::startOfExpression_helper(BackwardsScanner &tk, int index)
{
    if (tk[index - 1].isLiteral()) {
        return index - 1;
    } else if (tk[index - 1].is(T_THIS)) {
        return index - 1;
    } else if (tk[index - 1].is(T_TYPEID)) {
        return index - 1;
    } else if (tk[index - 1].is(T_SIGNAL)) {
        if (tk[index - 2].is(T_COMMA) && !_jumpedComma) {
            _jumpedComma = true;
            return startOfExpression(tk, index - 2);
        }
        return index - 1;
    } else if (tk[index - 1].is(T_SLOT)) {
        if (tk[index - 2].is(T_COMMA) && !_jumpedComma) {
            _jumpedComma = true;
            return startOfExpression(tk, index - 2);
        }
        return index - 1;
    } else if (tk[index - 1].is(T_IDENTIFIER)) {
        if (tk[index - 2].is(T_TILDE)) {
            if (tk[index - 3].is(T_COLON_COLON)) {
                return startOfExpression(tk, index - 3);
            } else if (tk[index - 3].is(T_DOT) || tk[index - 3].is(T_ARROW)) {
                return startOfExpression(tk, index - 3);
            }
            return index - 2;
        } else if (tk[index - 2].is(T_COLON_COLON)) {
            return startOfExpression(tk, index - 1);
        } else if (tk[index - 2].is(T_DOT) || tk[index - 2].is(T_ARROW)) {
            return startOfExpression(tk, index - 2);
        } else if (tk[index - 2].is(T_DOT_STAR) || tk[index - 2].is(T_ARROW_STAR)) {
            return startOfExpression(tk, index - 2);
        }
        return index - 1;
    } else if (tk[index - 1].is(T_RPAREN)) {
        int rparenIndex = tk.startOfMatchingBrace(index);
        if (rparenIndex != index) {
            if (tk[rparenIndex - 1].is(T_GREATER)) {
                int lessIndex = tk.startOfMatchingBrace(rparenIndex);
                if (lessIndex != rparenIndex - 1) {
                    if (tk[lessIndex - 1].is(T_DYNAMIC_CAST)     ||
                        tk[lessIndex - 1].is(T_STATIC_CAST)      ||
                        tk[lessIndex - 1].is(T_CONST_CAST)       ||
                        tk[lessIndex - 1].is(T_REINTERPRET_CAST))
                        return lessIndex - 1;
                    else if (tk[lessIndex - 1].is(T_IDENTIFIER))
                        return startOfExpression(tk, lessIndex);
                    else if (tk[lessIndex - 1].is(T_SIGNAL))
                        return startOfExpression(tk, lessIndex);
                    else if (tk[lessIndex - 1].is(T_SLOT))
                        return startOfExpression(tk, lessIndex);
                }
            }
            return startOfExpression(tk, rparenIndex);
        }
        return index;
    } else if (tk[index - 1].is(T_RBRACKET)) {
        int rbracketIndex = tk.startOfMatchingBrace(index);
        if (rbracketIndex != index)
            return startOfExpression(tk, rbracketIndex);
        return index;
    } else if (tk[index - 1].is(T_COLON_COLON)) {
        if (tk[index - 2].is(T_GREATER)) { // ### not exactly
            int lessIndex = tk.startOfMatchingBrace(index - 1);
            if (lessIndex != index - 1)
                return startOfExpression(tk, lessIndex);
            return index - 1;
        } else if (tk[index - 2].is(T_IDENTIFIER)) {
            return startOfExpression(tk, index - 1);
        }
        return index - 1;
    } else if (tk[index - 1].is(T_DOT) || tk[index - 1].is(T_ARROW)) {
        return startOfExpression(tk, index - 1);
    } else if (tk[index - 1].is(T_DOT_STAR) || tk[index - 1].is(T_ARROW_STAR)) {
        return startOfExpression(tk, index - 1);
    }

    return index;
}

bool ExpressionUnderCursor::isAccessToken(const SimpleToken &tk)
{
    switch (tk.kind()) {
    case T_COLON_COLON:
    case T_DOT:      case T_ARROW:
    case T_DOT_STAR: case T_ARROW_STAR:
        return true;
    default:
        return false;
    } // switch
}

QString ExpressionUnderCursor::operator()(const QTextCursor &cursor)
{
    BackwardsScanner scanner(cursor);

    _jumpedComma = false;

    const int initialSize = scanner.startToken();
    const int i = startOfExpression(scanner, initialSize);
    if (i == initialSize)
        return QString();

    return scanner.mid(i);
}

int ExpressionUnderCursor::startOfFunctionCall(const QTextCursor &cursor) const
{
    BackwardsScanner scanner(cursor);

    int index = scanner.startToken();

    forever {
        const SimpleToken &tk = scanner[index - 1];

        if (tk.is(T_EOF_SYMBOL))
            break;
        else if (tk.is(T_LPAREN))
            return scanner.startPosition() + tk.position();
        else if (tk.is(T_RPAREN)) {
            int matchingBrace = scanner.startOfMatchingBrace(index);

            if (matchingBrace == index) // If no matching brace found
                return -1;

            index = matchingBrace;
        } else
            --index;
    }

    return -1;
}
