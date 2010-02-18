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

#include "profileevaluator.h"
#include "proitems.h"

#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QList>
#include <QtCore/QRegExp>
#include <QtCore/QSet>
#include <QtCore/QStack>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>

#ifdef Q_OS_UNIX
#include <unistd.h>
#include <sys/utsname.h>
#else
#include <Windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>

#ifdef Q_OS_WIN32
#define QT_POPEN _popen
#define QT_PCLOSE _pclose
#else
#define QT_POPEN popen
#define QT_PCLOSE pclose
#endif

QT_BEGIN_NAMESPACE

static void refFunctions(QHash<QString, ProBlock *> *defs)
{
    foreach (ProBlock *itm, *defs)
        itm->ref();
}

static void clearFunctions(QHash<QString, ProBlock *> *defs)
{
    foreach (ProBlock *itm, *defs)
        itm->deref();
    defs->clear();
}

static void clearFunctions(ProFileEvaluator::FunctionDefs *defs)
{
    clearFunctions(&defs->replaceFunctions);
    clearFunctions(&defs->testFunctions);
}


///////////////////////////////////////////////////////////////////////
//
// Option
//
///////////////////////////////////////////////////////////////////////

ProFileEvaluator::Option::Option()
{
#ifdef Q_OS_WIN
    dirlist_sep = QLatin1Char(';');
    dir_sep = QLatin1Char('\\');
#else
    dirlist_sep = QLatin1Char(':');
    dir_sep = QLatin1Char('/');
#endif
    qmakespec = QString::fromLatin1(qgetenv("QMAKESPEC").data());

#if defined(Q_OS_WIN32)
    target_mode = TARG_WIN_MODE;
#elif defined(Q_OS_MAC)
    target_mode = TARG_MACX_MODE;
#elif defined(Q_OS_QNX6)
    target_mode = TARG_QNX6_MODE;
#else
    target_mode = TARG_UNIX_MODE;
#endif

    field_sep = QLatin1String(" ");
}

ProFileEvaluator::Option::~Option()
{
    clearFunctions(&base_functions);
}

QString ProFileEvaluator::Option::field_sep;

///////////////////////////////////////////////////////////////////////
//
// ProFileEvaluator::Private
//
///////////////////////////////////////////////////////////////////////

class ProFileEvaluator::Private : public AbstractProItemVisitor
{
public:
    Private(ProFileEvaluator *q_, ProFileEvaluator::Option *option);
    ~Private();

    ProFileEvaluator *q;
    int m_lineNo;                                   // Error reporting
    bool m_verbose;

    /////////////// Reading pro file

    bool read(ProFile *pro);
    bool read(ProBlock *pro, const QString &content);
    bool read(ProBlock *pro, QTextStream *ts);

    ProBlock *currentBlock();
    void updateItem(ushort *ptr);
    void updateItem2();
    bool insertVariable(ushort *ptr, bool *doSplit, bool *doSemicolon);
    void insertOperator(const char op);
    void insertComment(const QString &comment);
    void enterScope(bool multiLine);
    void leaveScope();
    void finalizeBlock();

    QStack<ProBlock *> m_blockstack;
    ProBlock *m_block;

    ProItem *m_commentItem;
    QString m_proitem;
    QString m_pendingComment;

    /////////////// Evaluating pro file contents

    // implementation of AbstractProItemVisitor
    ProItem::ProItemReturn visitBeginProBlock(ProBlock *block);
    void visitEndProBlock(ProBlock *block);
    ProItem::ProItemReturn visitProLoopIteration();
    void visitProLoopCleanup();
    void visitBeginProVariable(ProVariable *variable);
    void visitEndProVariable(ProVariable *variable);
    ProItem::ProItemReturn visitBeginProFile(ProFile *value);
    ProItem::ProItemReturn visitEndProFile(ProFile *value);
    void visitProValue(ProValue *value);
    ProItem::ProItemReturn visitProFunction(ProFunction *function);
    void visitProOperator(ProOperator *oper);
    void visitProCondition(ProCondition *condition);

    QStringList valuesDirect(const QString &variableName) const { return m_valuemap[variableName]; }
    QStringList values(const QString &variableName) const;
    QStringList values(const QString &variableName, const ProFile *pro) const;
    QStringList values(const QString &variableName, const QHash<QString, QStringList> &place,
                       const ProFile *pro) const;
    QString propertyValue(const QString &val, bool complain = true) const;

    QStringList split_value_list(const QString &vals, bool do_semicolon = false);
    QStringList split_arg_list(const QString &params);
    bool isActiveConfig(const QString &config, bool regex = false);
    QStringList expandVariableReferences(const QString &value);
    void doVariableReplace(QString *str);
    QStringList evaluateExpandFunction(const QString &function, const QString &arguments);
    QString format(const char *format) const;
    void logMessage(const QString &msg) const;
    void errorMessage(const QString &msg) const;
    void fileMessage(const QString &msg) const;

    QString currentFileName() const;
    QString currentDirectory() const;
    ProFile *currentProFile() const;

    ProItem::ProItemReturn evaluateConditionalFunction(const QString &function, const QString &arguments);
    bool evaluateFile(const QString &fileName);
    bool evaluateFeatureFile(const QString &fileName,
                             QHash<QString, QStringList> *values = 0, FunctionDefs *defs = 0);
    bool evaluateFileInto(const QString &fileName,
                          QHash<QString, QStringList> *values, FunctionDefs *defs);

    static inline ProItem::ProItemReturn returnBool(bool b)
        { return b ? ProItem::ReturnTrue : ProItem::ReturnFalse; }

    QList<QStringList> prepareFunctionArgs(const QString &arguments);
    QStringList evaluateFunction(ProBlock *funcPtr, const QList<QStringList> &argumentsList, bool *ok);

    QStringList qmakeMkspecPaths() const;
    QStringList qmakeFeaturePaths() const;

    struct State {
        bool condition;
        bool prevCondition;
        QStringList varVal;
    } m_sts;
    bool m_invertNext; // Short-lived, so not in State
    bool m_hadCondition; // Nested calls set it on return, so no need for it to be in State
    int m_skipLevel;
    bool m_cumulative;
    QStack<QString> m_oldPathStack;                 // To restore the current path to the path
    QStack<ProFile*> m_profileStack;                // To handle 'include(a.pri), so we can track back to 'a.pro' when finished with 'a.pri'
    struct ProLoop {
        QString variable;
        QStringList oldVarVal;
        QStringList list;
        int index;
        bool infinite;
    };
    QStack<ProLoop> m_loopStack;

    QHash<QString, QStringList> m_valuemap;         // VariableName must be us-ascii, the content however can be non-us-ascii.
    QHash<const ProFile*, QHash<QString, QStringList> > m_filevaluemap; // Variables per include file
    QString m_outputDir;

    bool m_definingTest;
    QString m_definingFunc;
    FunctionDefs m_functionDefs;
    QStringList m_returnValue;
    QStack<QHash<QString, QStringList> > m_valuemapStack;
    QStack<QHash<const ProFile*, QHash<QString, QStringList> > > m_filevaluemapStack;

    QStringList m_addUserConfigCmdArgs;
    QStringList m_removeUserConfigCmdArgs;
    bool m_parsePreAndPostFiles;

    Option *m_option;
};

#if !defined(__GNUC__) || __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ > 3)
Q_DECLARE_TYPEINFO(ProFileEvaluator::Private::State, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(ProFileEvaluator::Private::ProLoop, Q_MOVABLE_TYPE);
#endif

ProFileEvaluator::Private::Private(ProFileEvaluator *q_, ProFileEvaluator::Option *option)
  : q(q_), m_option(option)
{
    // Configuration, more or less
    m_verbose = true;
    m_cumulative = true;
    m_parsePreAndPostFiles = true;

    // Evaluator state
    m_sts.condition = false;
    m_sts.prevCondition = false;
    m_invertNext = false;
    m_skipLevel = 0;
    m_definingFunc.clear();
}

ProFileEvaluator::Private::~Private()
{
    clearFunctions(&m_functionDefs);
}

////////// Parser ///////////

bool ProFileEvaluator::Private::read(ProFile *pro)
{
    QFile file(pro->fileName());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMessage(format("%1 not readable.").arg(pro->fileName()));
        return false;
    }

    QTextStream ts(&file);
    m_lineNo = 1;
    return read(pro, &ts);
}

bool ProFileEvaluator::Private::read(ProBlock *pro, const QString &content)
{
    QString str(content);
    QTextStream ts(&str, QIODevice::ReadOnly | QIODevice::Text);
    m_lineNo = 1;
    return read(pro, &ts);
}

bool ProFileEvaluator::Private::read(ProBlock *pro, QTextStream *ts)
{
    // Parser state
    m_block = 0;
    m_commentItem = 0;
    m_blockstack.clear();
    m_blockstack.push(pro);

  freshLine:
    int parens = 0;
    bool inError = false;
    bool inAssignment = false;
    bool doSplit = false;
    bool doSemicolon = false;
    bool putSpace = false;
    ushort quote = 0;
    while (!ts->atEnd()) {
        QString line = ts->readLine();
        const ushort *cur = (const ushort *)line.unicode(),
                     *end = cur + line.length(),
                     *orgend = end,
                     *cmtptr = 0;
        ushort c, *ptr;

        // First, skip leading whitespace
        forever {
            if (cur == end) { // Entirely empty line (sans whitespace)
                updateItem2();
                finalizeBlock();
                ++m_lineNo;
                goto freshLine;
            }
            c = *cur;
            if (c != ' ' && c != '\t')
                break;
            cur++;
        }

        // Then strip comments. Yep - no escaping is possible.
        for (const ushort *cptr = cur; cptr != end; ++cptr)
            if (*cptr == '#') {
                if (cptr == cur) { // Line with only a comment (sans whitespace)
                    if (!inError)
                        insertComment(line.right(end - (cptr + 1)));
                    // Qmake bizarreness: such lines do not affect line continuations
                    goto ignore;
                }
                end = cptr;
                cmtptr = cptr + 1;
                break;
            }

        // Then look for line continuations
        bool lineCont;
        forever {
            // We don't have to check for underrun here, as we already determined
            // that the line is non-empty.
            ushort ec = *(end - 1);
            if (ec == '\\') {
                --end;
                lineCont = true;
                break;
            }
            if (ec != ' ' && ec != '\t') {
                lineCont = false;
                break;
            }
            --end;
        }

        if (!inError) {
            // May need enough space for this line and anything accumulated so far
            m_proitem.reserve(m_proitem.length() + (end - cur));

            // Finally, do the tokenization
            if (!inAssignment) {
              newItem:
                ptr = (ushort *)m_proitem.unicode() + m_proitem.length();
                do {
                    if (cur == end)
                        goto lineEnd;
                    c = *cur++;
                } while (c == ' ' || c == '\t');
                forever {
                    if (c == '"') {
                        quote = '"' - quote;
                    } else if (!quote) {
                        if (c == '(') {
                            ++parens;
                        } else if (c == ')') {
                            --parens;
                        } else if (!parens) {
                            if (c == ':') {
                                updateItem(ptr);
                                enterScope(false);
                              nextItem:
                                putSpace = false;
                                goto newItem;
                            }
                            if (c == '{') {
                                updateItem(ptr);
                                enterScope(true);
                                goto nextItem;
                            }
                            if (c == '}') {
                                updateItem(ptr);
                                leaveScope();
                                goto nextItem;
                            }
                            if (c == '=') {
                                if (insertVariable(ptr, &doSplit, &doSemicolon)) {
                                    inAssignment = true;
                                    putSpace = false;
                                    break;
                                }
                                inError = true;
                                goto skip;
                            }
                            if (c == '|' || c == '!') {
                                updateItem(ptr);
                                insertOperator(c);
                                goto nextItem;
                            }
                        }
                    }

                    if (putSpace) {
                        putSpace = false;
                        *ptr++ = ' ';
                    }
                    *ptr++ = c;

                    forever {
                        if (cur == end)
                            goto lineEnd;
                        c = *cur++;
                        if (c != ' ' && c != '\t')
                            break;
                        putSpace = true;
                    }
                }
            } // !inAssignment

          nextVal:
            ptr = (ushort *)m_proitem.unicode() + m_proitem.length();
            do {
                if (cur == end)
                    goto lineEnd;
                c = *cur++;
            } while (c == ' ' || c == '\t');
            if (doSplit) {
                // Qmake's parser supports truly bizarre quote nesting here, but later
                // stages (in qmake) don't grok it anyway. So make it simple instead.
                forever {
                    if (c == '\\') {
                        ushort ec;
                        if (cur != end && ((ec = *cur) == '"' || ec == '\'')) {
                            ++cur;
                            if (putSpace) {
                                putSpace = false;
                                *ptr++ = ' ';
                            }
                            *ptr++ = '\\';
                            *ptr++ = ec;
                            goto getNext;
                        }
                    } else {
                        if (quote) {
                            if (c == quote) {
                                quote = 0;
                            } else if (c == ' ' || c == '\t') {
                                putSpace = true;
                                goto getNext;
                            }
                        } else {
                            if (c == '"' || c == '\'') {
                                quote = c;
                            } else if (c == ')') {
                                --parens;
                            } else if (c == '(') {
                                ++parens;
                            } else if (c == ' ' || c == '\t') {
                                if (parens) {
                                    putSpace = true;
                                    goto getNext;
                                }
                                updateItem(ptr);
                                // assert(!putSpace);
                                goto nextVal;
                            }
                        }
                    }

                    if (putSpace) {
                        putSpace = false;
                        *ptr++ = ' ';
                    }
                    *ptr++ = c;

                  getNext:
                    if (cur == end) {
                        if (!quote && !parens)
                            goto flushItem;
                        break;
                    }
                    c = *cur++;
                }
            } else { // doSplit
                forever {
                    if (putSpace) {
                        putSpace = false;
                        *ptr++ = ' ';
                    }
                    *ptr++ = c;

                    forever {
                        if (cur == end)
                            goto lineEnd;
                        c = *cur++;
                        if (c != ' ' && c != '\t')
                            break;
                        putSpace = true;
                    }
                }
            }
          lineEnd:
            if (lineCont) {
                m_proitem.resize(ptr - (ushort *)m_proitem.unicode());
                putSpace = !m_proitem.isEmpty();
            } else {
              flushItem:
                updateItem(ptr);
                putSpace = false;
            }
            if (cmtptr)
                insertComment(line.right(orgend - cmtptr));
        } // !inError
      skip:
        if (!lineCont) {
            finalizeBlock();
            ++m_lineNo;
            goto freshLine;
        }
      ignore:
        ++m_lineNo;
    }
    m_proitem.clear(); // Throw away pre-allocation
    return true;
}

void ProFileEvaluator::Private::finalizeBlock()
{
    if (m_blockstack.top()->blockKind() & ProBlock::SingleLine)
        leaveScope();
    m_block = 0;
    m_commentItem = 0;
}

bool ProFileEvaluator::Private::insertVariable(ushort *ptr, bool *doSplit, bool *doSemicolon)
{
    ProVariable::VariableOperator opkind;
    ushort *uc = (ushort *)m_proitem.unicode();

    if (ptr == uc) // Line starting with '=', like a conflict marker
        return false;

    switch (*(ptr - 1)) {
        case '+':
            --ptr;
            opkind = ProVariable::AddOperator;
            break;
        case '-':
            --ptr;
            opkind = ProVariable::RemoveOperator;
            break;
        case '*':
            --ptr;
            opkind = ProVariable::UniqueAddOperator;
            break;
        case '~':
            --ptr;
            opkind = ProVariable::ReplaceOperator;
            break;
        default:
            opkind = ProVariable::SetOperator;
            goto skipTrunc;
    }

    if (ptr == uc) // Line starting with manipulation operator
        return false;
    if (*(ptr - 1) == ' ')
        --ptr;

  skipTrunc:
    m_proitem.resize(ptr - uc);
    QString proVar = m_proitem;
    proVar.detach();

    ProBlock *block = m_blockstack.top();
    ProVariable *variable = new ProVariable(proVar, block);
    variable->setLineNumber(m_lineNo);
    variable->setVariableOperator(opkind);
    block->appendItem(variable);
    m_block = variable;

    if (!m_pendingComment.isEmpty()) {
        m_block->setComment(m_pendingComment);
        m_pendingComment.clear();
    }
    m_commentItem = variable;

    m_proitem.resize(0);

    *doSplit = (opkind != ProVariable::ReplaceOperator);
    *doSemicolon = (proVar == QLatin1String("DEPENDPATH")
                    || proVar == QLatin1String("INCLUDEPATH"));
    return true;
}

void ProFileEvaluator::Private::insertOperator(const char op)
{
    ProOperator::OperatorKind opkind;
    switch (op) {
        case '!':
            opkind = ProOperator::NotOperator;
            break;
        case '|':
            opkind = ProOperator::OrOperator;
            break;
        default:
            opkind = ProOperator::OrOperator;
    }

    ProBlock * const block = currentBlock();
    ProOperator * const proOp = new ProOperator(opkind);
    proOp->setLineNumber(m_lineNo);
    block->appendItem(proOp);
    m_commentItem = proOp;
}

void ProFileEvaluator::Private::insertComment(const QString &comment)
{
    QString strComment;
    if (!m_commentItem)
        strComment = m_pendingComment;
    else
        strComment = m_commentItem->comment();

    if (strComment.isEmpty())
        strComment = comment;
    else {
        strComment += QLatin1Char('\n');
        strComment += comment.trimmed();
    }

    strComment = strComment.trimmed();

    if (!m_commentItem)
        m_pendingComment = strComment;
    else
        m_commentItem->setComment(strComment);
}

void ProFileEvaluator::Private::enterScope(bool multiLine)
{
    ProBlock *parent = currentBlock();
    ProBlock *block = new ProBlock(parent);
    block->setLineNumber(m_lineNo);
    parent->setBlockKind(ProBlock::ScopeKind);

    parent->appendItem(block);

    if (multiLine)
        block->setBlockKind(ProBlock::ScopeContentsKind);
    else
        block->setBlockKind(ProBlock::ScopeContentsKind|ProBlock::SingleLine);

    m_blockstack.push(block);
    m_block = 0;
}

void ProFileEvaluator::Private::leaveScope()
{
    if (m_blockstack.count() == 1)
        errorMessage(format("Excess closing brace."));
    else
        m_blockstack.pop();
    finalizeBlock();
}

ProBlock *ProFileEvaluator::Private::currentBlock()
{
    if (m_block)
        return m_block;

    ProBlock *parent = m_blockstack.top();
    m_block = new ProBlock(parent);
    m_block->setLineNumber(m_lineNo);
    parent->appendItem(m_block);

    if (!m_pendingComment.isEmpty()) {
        m_block->setComment(m_pendingComment);
        m_pendingComment.clear();
    }

    m_commentItem = m_block;

    return m_block;
}

void ProFileEvaluator::Private::updateItem(ushort *ptr)
{
    m_proitem.resize(ptr - (ushort *)m_proitem.unicode());
    updateItem2();
}

void ProFileEvaluator::Private::updateItem2()
{
    if (m_proitem.isEmpty())
        return;

    QString proItem = m_proitem;
    proItem.detach();

    ProBlock *block = currentBlock();
    if (block->blockKind() & ProBlock::VariableKind) {
        m_commentItem = new ProValue(proItem, static_cast<ProVariable*>(block));
    } else if (proItem.endsWith(QLatin1Char(')'))) {
        m_commentItem = new ProFunction(proItem);
    } else {
        m_commentItem = new ProCondition(proItem);
    }
    m_commentItem->setLineNumber(m_lineNo);
    block->appendItem(m_commentItem);

    m_proitem.resize(0);
}

//////// Evaluator tools /////////

QStringList ProFileEvaluator::Private::split_arg_list(const QString &params)
{
    int quote = 0;
    QStringList args;

    const ushort LPAREN = '(';
    const ushort RPAREN = ')';
    const ushort SINGLEQUOTE = '\'';
    const ushort DOUBLEQUOTE = '"';
    const ushort COMMA = ',';
    const ushort SPACE = ' ';
    //const ushort TAB = '\t';

    ushort unicode;
    const QChar *params_data = params.data();
    const int params_len = params.length();
    int last = 0;
    while (last < params_len && ((params_data+last)->unicode() == SPACE
                                /*|| (params_data+last)->unicode() == TAB*/))
        ++last;
    for (int x = last, parens = 0; x <= params_len; x++) {
        unicode = (params_data+x)->unicode();
        if (x == params_len) {
            while (x && (params_data+(x-1))->unicode() == SPACE)
                --x;
            QString mid(params_data+last, x-last);
            if (quote) {
                if (mid[0] == quote && mid[(int)mid.length()-1] == quote)
                    mid = mid.mid(1, mid.length()-2);
                quote = 0;
            }
            args << mid;
            break;
        }
        if (unicode == LPAREN) {
            --parens;
        } else if (unicode == RPAREN) {
            ++parens;
        } else if (quote && unicode == quote) {
            quote = 0;
        } else if (!quote && (unicode == SINGLEQUOTE || unicode == DOUBLEQUOTE)) {
            quote = unicode;
        }
        if (!parens && !quote && unicode == COMMA) {
            QString mid = params.mid(last, x - last).trimmed();
            args << mid;
            last = x+1;
            while (last < params_len && ((params_data+last)->unicode() == SPACE
                                        /*|| (params_data+last)->unicode() == TAB*/))
                ++last;
        }
    }
    return args;
}

QStringList ProFileEvaluator::Private::split_value_list(const QString &vals, bool do_semicolon)
{
    QString build;
    QStringList ret;
    QStack<char> quote;

    const ushort SPACE = ' ';
    const ushort LPAREN = '(';
    const ushort RPAREN = ')';
    const ushort SINGLEQUOTE = '\'';
    const ushort DOUBLEQUOTE = '"';
    const ushort BACKSLASH = '\\';
    const ushort SEMICOLON = ';';

    ushort unicode;
    const QChar *vals_data = vals.data();
    const int vals_len = vals.length();
    for (int x = 0, parens = 0; x < vals_len; x++) {
        unicode = vals_data[x].unicode();
        if (x != (int)vals_len-1 && unicode == BACKSLASH &&
            (vals_data[x+1].unicode() == SINGLEQUOTE || vals_data[x+1].unicode() == DOUBLEQUOTE)) {
            build += vals_data[x++]; //get that 'escape'
        } else if (!quote.isEmpty() && unicode == quote.top()) {
            quote.pop();
        } else if (unicode == SINGLEQUOTE || unicode == DOUBLEQUOTE) {
            quote.push(unicode);
        } else if (unicode == RPAREN) {
            --parens;
        } else if (unicode == LPAREN) {
            ++parens;
        }

        if (!parens && quote.isEmpty() && ((do_semicolon && unicode == SEMICOLON) ||
                                           vals_data[x] == SPACE)) {
            ret << build;
            build.clear();
        } else {
            build += vals_data[x];
        }
    }
    if (!build.isEmpty())
        ret << build;
    return ret;
}

static void insertUnique(QHash<QString, QStringList> *map,
    const QString &key, const QStringList &value)
{
    QStringList &sl = (*map)[key];
    foreach (const QString &str, value)
        if (!sl.contains(str))
            sl.append(str);
}

static void removeEach(QHash<QString, QStringList> *map,
    const QString &key, const QStringList &value)
{
    QStringList &sl = (*map)[key];
    foreach (const QString &str, value)
        sl.removeAll(str);
}

static void replaceInList(QStringList *varlist,
        const QRegExp &regexp, const QString &replace, bool global)
{
    for (QStringList::Iterator varit = varlist->begin(); varit != varlist->end(); ) {
        if ((*varit).contains(regexp)) {
            (*varit).replace(regexp, replace);
            if ((*varit).isEmpty())
                varit = varlist->erase(varit);
            else
                ++varit;
            if (!global)
                break;
        } else {
            ++varit;
        }
    }
}

static QString expandEnvVars(const QString &str)
{
    QString string = str;
    int rep;
    QRegExp reg_variableName(QLatin1String("\\$\\(.*\\)"));
    reg_variableName.setMinimal(true);
    while ((rep = reg_variableName.indexIn(string)) != -1)
        string.replace(rep, reg_variableName.matchedLength(),
                       QString::fromLocal8Bit(qgetenv(string.mid(rep + 2, reg_variableName.matchedLength() - 3).toLatin1().constData()).constData()));
    return string;
}

static QStringList expandEnvVars(const QStringList &x)
{
    QStringList ret;
    foreach (const QString &str, x)
        ret << expandEnvVars(str);
    return ret;
}

// This is braindead, but we want qmake compat
static QString fixPathToLocalOS(const QString &str)
{
    QString string = str;

    if (string.length() > 2 && string.at(0).isLetter() && string.at(1) == QLatin1Char(':'))
        string[0] = string[0].toLower();

#if defined(Q_OS_WIN32)
    string.replace(QLatin1Char('/'), QLatin1Char('\\'));
#else
    string.replace(QLatin1Char('\\'), QLatin1Char('/'));
#endif
    return string;
}

//////// Evaluator /////////

ProItem::ProItemReturn ProFileEvaluator::Private::visitBeginProBlock(ProBlock *block)
{
    if (block->blockKind() & ProBlock::ScopeContentsKind) {
        if (!m_definingFunc.isEmpty()) {
            if (!m_skipLevel || m_cumulative) {
                QHash<QString, ProBlock *> *hash =
                        (m_definingTest ? &m_functionDefs.testFunctions
                                        : &m_functionDefs.replaceFunctions);
                if (ProBlock *def = hash->value(m_definingFunc))
                    def->deref();
                hash->insert(m_definingFunc, block);
                block->ref();
                block->setBlockKind(block->blockKind() | ProBlock::FunctionBodyKind);
            }
            m_definingFunc.clear();
            return ProItem::ReturnSkip;
        } else if (!(block->blockKind() & ProBlock::FunctionBodyKind)) {
            if (!m_sts.condition) {
                if (m_skipLevel || m_hadCondition)
                    ++m_skipLevel;
            } else {
                Q_ASSERT(!m_skipLevel);
            }
        }
    } else {
        m_hadCondition = false;
        if (!m_skipLevel) {
            if (m_sts.condition) {
                m_sts.prevCondition = true;
                m_sts.condition = false;
            }
        } else {
            Q_ASSERT(!m_sts.condition);
        }
    }
    return ProItem::ReturnTrue;
}

void ProFileEvaluator::Private::visitEndProBlock(ProBlock *block)
{
    if ((block->blockKind() & ProBlock::ScopeContentsKind)
        && !(block->blockKind() & ProBlock::FunctionBodyKind)) {
        if (m_skipLevel) {
            Q_ASSERT(!m_sts.condition);
            --m_skipLevel;
        } else if (!(block->blockKind() & ProBlock::SingleLine)) {
            // Conditionals contained inside this block may have changed the state.
            // So we reset it here to make an else following us do the right thing.
            m_sts.condition = true;
        }
    }
}

ProItem::ProItemReturn ProFileEvaluator::Private::visitProLoopIteration()
{
    ProLoop &loop = m_loopStack.top();

    if (loop.infinite) {
        if (!loop.variable.isEmpty())
            m_valuemap[loop.variable] = QStringList(QString::number(loop.index++));
        if (loop.index > 1000) {
            errorMessage(format("ran into infinite loop (> 1000 iterations)."));
            return ProItem::ReturnFalse;
        }
    } else {
        QString val;
        do {
            if (loop.index >= loop.list.count())
                return ProItem::ReturnFalse;
            val = loop.list.at(loop.index++);
        } while (val.isEmpty()); // stupid, but qmake is like that
        m_valuemap[loop.variable] = QStringList(val);
    }
    return ProItem::ReturnTrue;
}

void ProFileEvaluator::Private::visitProLoopCleanup()
{
    ProLoop &loop = m_loopStack.top();
    m_valuemap[loop.variable] = loop.oldVarVal;
    m_loopStack.pop_back();
}

void ProFileEvaluator::Private::visitBeginProVariable(ProVariable *)
{
    m_sts.varVal.clear();
}

void ProFileEvaluator::Private::visitEndProVariable(ProVariable *variable)
{
    QString varName = variable->variable();

    switch (variable->variableOperator()) {
        case ProVariable::SetOperator:          // =
            if (!m_cumulative) {
                if (!m_skipLevel) {
                    m_valuemap[varName] = m_sts.varVal;
                    m_filevaluemap[currentProFile()][varName] = m_sts.varVal;
                }
            } else {
                // We are greedy for values.
                m_valuemap[varName] += m_sts.varVal;
                m_filevaluemap[currentProFile()][varName] += m_sts.varVal;
            }
            break;
        case ProVariable::UniqueAddOperator:    // *=
            if (!m_skipLevel || m_cumulative) {
                insertUnique(&m_valuemap, varName, m_sts.varVal);
                insertUnique(&m_filevaluemap[currentProFile()], varName, m_sts.varVal);
            }
            break;
        case ProVariable::AddOperator:          // +=
            if (!m_skipLevel || m_cumulative) {
                m_valuemap[varName] += m_sts.varVal;
                m_filevaluemap[currentProFile()][varName] += m_sts.varVal;
            }
            break;
        case ProVariable::RemoveOperator:       // -=
            if (!m_cumulative) {
                if (!m_skipLevel) {
                    removeEach(&m_valuemap, varName, m_sts.varVal);
                    removeEach(&m_filevaluemap[currentProFile()], varName, m_sts.varVal);
                }
            } else {
                // We are stingy with our values, too.
            }
            break;
        case ProVariable::ReplaceOperator:      // ~=
            {
                // DEFINES ~= s/a/b/?[gqi]

                QString val = m_sts.varVal.first();
                doVariableReplace(&val);
                if (val.length() < 4 || val.at(0) != QLatin1Char('s')) {
                    logMessage(format("the ~= operator can handle only the s/// function."));
                    break;
                }
                QChar sep = val.at(1);
                QStringList func = val.split(sep);
                if (func.count() < 3 || func.count() > 4) {
                    logMessage(format("the s/// function expects 3 or 4 arguments."));
                    break;
                }

                bool global = false, quote = false, case_sense = false;
                if (func.count() == 4) {
                    global = func[3].indexOf(QLatin1Char('g')) != -1;
                    case_sense = func[3].indexOf(QLatin1Char('i')) == -1;
                    quote = func[3].indexOf(QLatin1Char('q')) != -1;
                }
                QString pattern = func[1];
                QString replace = func[2];
                if (quote)
                    pattern = QRegExp::escape(pattern);

                QRegExp regexp(pattern, case_sense ? Qt::CaseSensitive : Qt::CaseInsensitive);

                if (!m_skipLevel || m_cumulative) {
                    // We could make a union of modified and unmodified values,
                    // but this will break just as much as it fixes, so leave it as is.
                    replaceInList(&m_valuemap[varName], regexp, replace, global);
                    replaceInList(&m_filevaluemap[currentProFile()][varName], regexp, replace, global);

                }
            }
            break;
    }
}

void ProFileEvaluator::Private::visitProOperator(ProOperator *oper)
{
    m_invertNext = (oper->operatorKind() == ProOperator::NotOperator);
}

void ProFileEvaluator::Private::visitProCondition(ProCondition *cond)
{
    if (!m_skipLevel) {
        m_hadCondition = true;
        if (!cond->text().compare(QLatin1String("else"), Qt::CaseInsensitive)) {
            m_sts.condition = !m_sts.prevCondition;
        } else {
            m_sts.prevCondition = false;
            if (!m_sts.condition && isActiveConfig(cond->text(), true) ^ m_invertNext)
                m_sts.condition = true;
        }
    }
    m_invertNext = false;
}

ProItem::ProItemReturn ProFileEvaluator::Private::visitBeginProFile(ProFile * pro)
{
    m_lineNo = pro->lineNumber();

    m_oldPathStack.push(QDir::currentPath());
    if (!QDir::setCurrent(pro->directoryName())) {
        m_oldPathStack.pop();
        return ProItem::ReturnFalse;
    }

    m_profileStack.push(pro);
    if (m_profileStack.count() == 1) {
        // Do this only for the initial profile we visit, since
        // that is *the* profile. All the other times we reach this function will be due to
        // include(file) or load(file)

        if (m_parsePreAndPostFiles) {

            if (m_option->base_valuemap.isEmpty()) {
                // ### init QMAKE_QMAKE, QMAKE_SH
                // ### init QMAKE_EXT_{C,H,CPP,OBJ}
                // ### init TEMPLATE_PREFIX

                QString qmake_cache = m_option->cachefile;
                if (qmake_cache.isEmpty() && !m_outputDir.isEmpty())  { //find it as it has not been specified
                    QDir dir(m_outputDir);
                    forever {
                        qmake_cache = dir.filePath(QLatin1String(".qmake.cache"));
                        if (QFile::exists(qmake_cache))
                            break;
                        if (!dir.cdUp() || dir.isRoot()) {
                            qmake_cache.clear();
                            break;
                        }
                    }
                }
                if (!qmake_cache.isEmpty()) {
                    qmake_cache = QDir::cleanPath(qmake_cache);
                    QHash<QString, QStringList> cache_valuemap;
                    if (evaluateFileInto(qmake_cache, &cache_valuemap, 0)) {
                        m_option->cachefile = qmake_cache;
                        if (m_option->qmakespec.isEmpty()) {
                            const QStringList &vals = cache_valuemap.value(QLatin1String("QMAKESPEC"));
                            if (!vals.isEmpty())
                                m_option->qmakespec = vals.first();
                        }
                    }
                }

                QStringList mkspec_roots = qmakeMkspecPaths();

                QString qmakespec = expandEnvVars(m_option->qmakespec);
                if (qmakespec.isEmpty()) {
                    foreach (const QString &root, mkspec_roots) {
                        QString mkspec = root + QLatin1String("/default");
                        QFileInfo default_info(mkspec);
                        if (default_info.exists() && default_info.isDir()) {
                            qmakespec = mkspec;
                            break;
                        }
                    }
                    if (qmakespec.isEmpty()) {
                        errorMessage(format("Could not find qmake configuration directory"));
                        // Unlike in qmake, not finding the spec is not critical ...
                    }
                }

                if (QDir::isRelativePath(qmakespec)) {
                    if (QFile::exists(qmakespec + QLatin1String("/qmake.conf"))) {
                        qmakespec = QFileInfo(qmakespec).absoluteFilePath();
                    } else if (!m_outputDir.isEmpty()
                               && QFile::exists(m_outputDir + QLatin1Char('/') + qmakespec
                                                + QLatin1String("/qmake.conf"))) {
                        qmakespec = m_outputDir + QLatin1Char('/') + qmakespec;
                    } else {
                        foreach (const QString &root, mkspec_roots) {
                            QString mkspec = root + QLatin1Char('/') + qmakespec;
                            if (QFile::exists(mkspec)) {
                                qmakespec = mkspec;
                                goto cool;
                            }
                        }
                        errorMessage(format("Could not find qmake configuration file"));
                        // Unlike in qmake, a missing config is not critical ...
                        qmakespec.clear();
                      cool: ;
                    }
                }

                if (!qmakespec.isEmpty()) {
                    m_option->qmakespec = QDir::cleanPath(qmakespec);

                    QString spec = m_option->qmakespec + QLatin1String("/qmake.conf");
                    if (!evaluateFileInto(spec,
                                          &m_option->base_valuemap, &m_option->base_functions)) {
                        errorMessage(format("Could not read qmake configuration file %1").arg(spec));
                    } else if (!m_option->cachefile.isEmpty()) {
                        evaluateFileInto(m_option->cachefile,
                                         &m_option->base_valuemap, &m_option->base_functions);
                    }
                }

                evaluateFeatureFile(QLatin1String("default_pre.prf"),
                                    &m_option->base_valuemap, &m_option->base_functions);
            }

            m_valuemap = m_option->base_valuemap;

            clearFunctions(&m_functionDefs);
            m_functionDefs = m_option->base_functions;
            refFunctions(&m_functionDefs.testFunctions);
            refFunctions(&m_functionDefs.replaceFunctions);

            QStringList &tgt = m_valuemap[QLatin1String("TARGET")];
            if (tgt.isEmpty())
                tgt.append(QFileInfo(pro->fileName()).baseName());

            QStringList &tmp = m_valuemap[QLatin1String("CONFIG")];
            tmp.append(m_addUserConfigCmdArgs);
            foreach (const QString &remove, m_removeUserConfigCmdArgs)
                tmp.removeAll(remove);
        }
    }

    return ProItem::ReturnTrue;
}

ProItem::ProItemReturn ProFileEvaluator::Private::visitEndProFile(ProFile * pro)
{
    m_lineNo = pro->lineNumber();

    if (m_profileStack.count() == 1) {
        if (m_parsePreAndPostFiles) {
            evaluateFeatureFile(QLatin1String("default_post.prf"));

            QSet<QString> processed;
            forever {
                bool finished = true;
                QStringList configs = valuesDirect(QLatin1String("CONFIG"));
                for (int i = configs.size() - 1; i >= 0; --i) {
                    const QString config = configs.at(i).toLower();
                    if (!processed.contains(config)) {
                        processed.insert(config);
                        if (evaluateFeatureFile(config)) {
                            finished = false;
                            break;
                        }
                    }
                }
                if (finished)
                    break;
            }
        }
    }
    m_profileStack.pop();

    return returnBool(QDir::setCurrent(m_oldPathStack.pop()));
}

void ProFileEvaluator::Private::visitProValue(ProValue *value)
{
    m_lineNo = value->lineNumber();
    m_sts.varVal += expandVariableReferences(value->value());
}

ProItem::ProItemReturn ProFileEvaluator::Private::visitProFunction(ProFunction *func)
{
    // Make sure that called subblocks don't inherit & destroy the state
    bool invertThis = m_invertNext;
    m_invertNext = false;
    if (!m_skipLevel) {
        m_hadCondition = true;
        m_sts.prevCondition = false;
    }
    if (m_cumulative || !m_sts.condition) {
        QString text = func->text();
        int lparen = text.indexOf(QLatin1Char('('));
        int rparen = text.lastIndexOf(QLatin1Char(')'));
        Q_ASSERT(lparen < rparen);
        QString arguments = text.mid(lparen + 1, rparen - lparen - 1);
        QString funcName = text.left(lparen);
        m_lineNo = func->lineNumber();
        ProItem::ProItemReturn result = evaluateConditionalFunction(funcName.trimmed(), arguments);
        if (result != ProItem::ReturnFalse && result != ProItem::ReturnTrue)
            return result;
        if (!m_skipLevel && ((result == ProItem::ReturnTrue) ^ invertThis))
            m_sts.condition = true;
    }
    return ProItem::ReturnTrue;
}


QStringList ProFileEvaluator::Private::qmakeMkspecPaths() const
{
    QStringList ret;
    const QString concat = QLatin1String("/mkspecs");

    QByteArray qmakepath = qgetenv("QMAKEPATH");
    if (!qmakepath.isEmpty())
        foreach (const QString &it, QString::fromLocal8Bit(qmakepath).split(m_option->dirlist_sep))
            ret << QDir::cleanPath(it) + concat;

    ret << propertyValue(QLatin1String("QT_INSTALL_DATA")) + concat;

    return ret;
}

QStringList ProFileEvaluator::Private::qmakeFeaturePaths() const
{
    QString mkspecs_concat = QLatin1String("/mkspecs");
    QString features_concat = QLatin1String("/features");
    QStringList concat;
    switch (m_option->target_mode) {
    case Option::TARG_MACX_MODE:
        concat << QLatin1String("/features/mac");
        concat << QLatin1String("/features/macx");
        concat << QLatin1String("/features/unix");
        break;
    case Option::TARG_UNIX_MODE:
        concat << QLatin1String("/features/unix");
        break;
    case Option::TARG_WIN_MODE:
        concat << QLatin1String("/features/win32");
        break;
    case Option::TARG_MAC9_MODE:
        concat << QLatin1String("/features/mac");
        concat << QLatin1String("/features/mac9");
        break;
    case Option::TARG_QNX6_MODE:
        concat << QLatin1String("/features/qnx6");
        concat << QLatin1String("/features/unix");
        break;
    }
    concat << features_concat;

    QStringList feature_roots;

    QByteArray mkspec_path = qgetenv("QMAKEFEATURES");
    if (!mkspec_path.isEmpty())
        foreach (const QString &f, QString::fromLocal8Bit(mkspec_path).split(m_option->dirlist_sep))
            feature_roots += QDir::cleanPath(f);

    feature_roots += propertyValue(QLatin1String("QMAKEFEATURES"), false).split(
            m_option->dirlist_sep, QString::SkipEmptyParts);

    if (!m_option->cachefile.isEmpty()) {
        QString path;
        int last_slash = m_option->cachefile.lastIndexOf((ushort)'/');
        if (last_slash != -1)
            path = m_option->cachefile.left(last_slash);
        foreach (const QString &concat_it, concat)
            feature_roots << (path + concat_it);
    }

    QByteArray qmakepath = qgetenv("QMAKEPATH");
    if (!qmakepath.isNull()) {
        const QStringList lst = QString::fromLocal8Bit(qmakepath).split(m_option->dirlist_sep);
        foreach (const QString &item, lst) {
            QString citem = QDir::cleanPath(item);
            foreach (const QString &concat_it, concat)
                feature_roots << (citem + mkspecs_concat + concat_it);
        }
    }

    if (!m_option->qmakespec.isEmpty()) {
        feature_roots << (m_option->qmakespec + features_concat);

        QDir specdir(m_option->qmakespec);
        while (!specdir.isRoot()) {
            if (!specdir.cdUp() || specdir.isRoot())
                break;
            if (QFile::exists(specdir.path() + features_concat)) {
                foreach (const QString &concat_it, concat)
                    feature_roots << (specdir.path() + concat_it);
                break;
            }
        }
    }

    foreach (const QString &concat_it, concat)
        feature_roots << (propertyValue(QLatin1String("QT_INSTALL_PREFIX")) +
                          mkspecs_concat + concat_it);
    foreach (const QString &concat_it, concat)
        feature_roots << (propertyValue(QLatin1String("QT_INSTALL_DATA")) +
                          mkspecs_concat + concat_it);

    for (int i = 0; i < feature_roots.count(); ++i)
        if (!feature_roots.at(i).endsWith((ushort)'/'))
            feature_roots[i].append((ushort)'/');

    return feature_roots;
}

QString ProFileEvaluator::Private::propertyValue(const QString &name, bool complain) const
{
    if (m_option->properties.contains(name))
        return m_option->properties.value(name);
    if (name == QLatin1String("QMAKE_MKSPECS"))
        return qmakeMkspecPaths().join(m_option->dirlist_sep);
    if (name == QLatin1String("QMAKE_VERSION"))
        return QLatin1String("1.0");        //### FIXME
    if (complain)
        logMessage(format("Querying unknown property %1").arg(name));
    return QString();
}

ProFile *ProFileEvaluator::Private::currentProFile() const
{
    if (m_profileStack.count() > 0)
        return m_profileStack.top();
    return 0;
}

QString ProFileEvaluator::Private::currentFileName() const
{
    ProFile *pro = currentProFile();
    if (pro)
        return pro->fileName();
    return QString();
}

QString ProFileEvaluator::Private::currentDirectory() const
{
    ProFile *cur = m_profileStack.top();
    return cur->directoryName();
}

void ProFileEvaluator::Private::doVariableReplace(QString *str)
{
    *str = expandVariableReferences(*str).join(Option::field_sep);
}

QStringList ProFileEvaluator::Private::expandVariableReferences(const QString &str)
{
    QStringList ret;
//    if (ok)
//        *ok = true;
    if (str.isEmpty())
        return ret;

    const ushort LSQUARE = '[';
    const ushort RSQUARE = ']';
    const ushort LCURLY = '{';
    const ushort RCURLY = '}';
    const ushort LPAREN = '(';
    const ushort RPAREN = ')';
    const ushort DOLLAR = '$';
    const ushort BACKSLASH = '\\';
    const ushort UNDERSCORE = '_';
    const ushort DOT = '.';
    const ushort SPACE = ' ';
    const ushort TAB = '\t';
    const ushort SINGLEQUOTE = '\'';
    const ushort DOUBLEQUOTE = '"';

    ushort unicode, quote = 0;
    const QChar *str_data = str.data();
    const int str_len = str.length();

    ushort term;
    QString var, args;

    int replaced = 0;
    QString current;
    for (int i = 0; i < str_len; ++i) {
        unicode = str_data[i].unicode();
        const int start_var = i;
        if (unicode == DOLLAR && str_len > i+2) {
            unicode = str_data[++i].unicode();
            if (unicode == DOLLAR) {
                term = 0;
                var.clear();
                args.clear();
                enum { VAR, ENVIRON, FUNCTION, PROPERTY } var_type = VAR;
                unicode = str_data[++i].unicode();
                if (unicode == LSQUARE) {
                    unicode = str_data[++i].unicode();
                    term = RSQUARE;
                    var_type = PROPERTY;
                } else if (unicode == LCURLY) {
                    unicode = str_data[++i].unicode();
                    var_type = VAR;
                    term = RCURLY;
                } else if (unicode == LPAREN) {
                    unicode = str_data[++i].unicode();
                    var_type = ENVIRON;
                    term = RPAREN;
                }
                forever {
                    if (!(unicode & (0xFF<<8)) &&
                       unicode != DOT && unicode != UNDERSCORE &&
                       //unicode != SINGLEQUOTE && unicode != DOUBLEQUOTE &&
                       (unicode < 'a' || unicode > 'z') && (unicode < 'A' || unicode > 'Z') &&
                       (unicode < '0' || unicode > '9'))
                        break;
                    var.append(QChar(unicode));
                    if (++i == str_len)
                        break;
                    unicode = str_data[i].unicode();
                    // at this point, i points to either the 'term' or 'next' character (which is in unicode)
                }
                if (var_type == VAR && unicode == LPAREN) {
                    var_type = FUNCTION;
                    int depth = 0;
                    forever {
                        if (++i == str_len)
                            break;
                        unicode = str_data[i].unicode();
                        if (unicode == LPAREN) {
                            depth++;
                        } else if (unicode == RPAREN) {
                            if (!depth)
                                break;
                            --depth;
                        }
                        args.append(QChar(unicode));
                    }
                    if (++i < str_len)
                        unicode = str_data[i].unicode();
                    else
                        unicode = 0;
                    // at this point i is pointing to the 'next' character (which is in unicode)
                    // this might actually be a term character since you can do $${func()}
                }
                if (term) {
                    if (unicode != term) {
                        logMessage(format("Missing %1 terminator [found %2]")
                            .arg(QChar(term))
                            .arg(unicode ? QString(unicode) : QString::fromLatin1(("end-of-line"))));
//                        if (ok)
//                            *ok = false;
                        return QStringList();
                    }
                } else {
                    // move the 'cursor' back to the last char of the thing we were looking at
                    --i;
                }
                // since i never points to the 'next' character, there is no reason for this to be set
                unicode = 0;

                QStringList replacement;
                if (var_type == ENVIRON) {
                    replacement = split_value_list(QString::fromLocal8Bit(qgetenv(var.toLatin1().constData())));
                } else if (var_type == PROPERTY) {
                    replacement << propertyValue(var);
                } else if (var_type == FUNCTION) {
                    replacement << evaluateExpandFunction(var, args);
                } else if (var_type == VAR) {
                    replacement = values(var);
                }
                if (!(replaced++) && start_var)
                    current = str.left(start_var);
                if (!replacement.isEmpty()) {
                    if (quote) {
                        current += replacement.join(Option::field_sep);
                    } else {
                        current += replacement.takeFirst();
                        if (!replacement.isEmpty()) {
                            if (!current.isEmpty())
                                ret.append(current);
                            current = replacement.takeLast();
                            if (!replacement.isEmpty())
                                ret += replacement;
                        }
                    }
                }
            } else {
                if (replaced)
                    current.append(QLatin1Char('$'));
            }
        }
        if (quote && unicode == quote) {
            unicode = 0;
            quote = 0;
        } else if (unicode == BACKSLASH) {
            bool escape = false;
            const char *symbols = "[]{}()$\\'\"";
            for (const char *s = symbols; *s; ++s) {
                if (str_data[i+1].unicode() == (ushort)*s) {
                    i++;
                    escape = true;
                    if (!(replaced++))
                        current = str.left(start_var);
                    current.append(str.at(i));
                    break;
                }
            }
            if (escape || !replaced)
                unicode =0;
        } else if (!quote && (unicode == SINGLEQUOTE || unicode == DOUBLEQUOTE)) {
            quote = unicode;
            unicode = 0;
            if (!(replaced++) && i)
                current = str.left(i);
        } else if (!quote && (unicode == SPACE || unicode == TAB)) {
            unicode = 0;
            if (!current.isEmpty()) {
                ret.append(current);
                current.clear();
            }
        }
        if (replaced && unicode)
            current.append(QChar(unicode));
    }
    if (!replaced)
        ret = QStringList(str);
    else if (!current.isEmpty())
        ret.append(current);
    return ret;
}

bool ProFileEvaluator::Private::isActiveConfig(const QString &config, bool regex)
{
    // magic types for easy flipping
    if (config == QLatin1String("true"))
        return true;
    if (config == QLatin1String("false"))
        return false;

    // mkspecs
    if ((m_option->target_mode == m_option->TARG_MACX_MODE
            || m_option->target_mode == m_option->TARG_QNX6_MODE
            || m_option->target_mode == m_option->TARG_UNIX_MODE)
          && config == QLatin1String("unix"))
        return true;
    if (m_option->target_mode == m_option->TARG_MACX_MODE && config == QLatin1String("macx"))
        return true;
    if (m_option->target_mode == m_option->TARG_QNX6_MODE && config == QLatin1String("qnx6"))
        return true;
    if (m_option->target_mode == m_option->TARG_MAC9_MODE && config == QLatin1String("mac9"))
        return true;
    if ((m_option->target_mode == m_option->TARG_MAC9_MODE
            || m_option->target_mode == m_option->TARG_MACX_MODE)
          && config == QLatin1String("mac"))
        return true;
    if (m_option->target_mode == m_option->TARG_WIN_MODE && config == QLatin1String("win32"))
        return true;

    if (regex) {
        QRegExp re(config, Qt::CaseSensitive, QRegExp::Wildcard);

        if (re.exactMatch(m_option->qmakespec))
            return true;

        // CONFIG variable
        foreach (const QString &configValue, m_valuemap.value(QLatin1String("CONFIG"))) {
            if (re.exactMatch(configValue))
                return true;
        }
    } else {
        // mkspecs
        if (m_option->qmakespec == config)
            return true;

        // CONFIG variable
        foreach (const QString &configValue, m_valuemap.value(QLatin1String("CONFIG"))) {
            if (configValue == config)
                return true;
        }
    }

    return false;
}

QList<QStringList> ProFileEvaluator::Private::prepareFunctionArgs(const QString &arguments)
{
    QList<QStringList> args_list;
    foreach (const QString &urArg, split_arg_list(arguments)) {
        QStringList tmp;
        foreach (const QString &arg, split_value_list(urArg))
            tmp += expandVariableReferences(arg);
        args_list << tmp;
    }
    return args_list;
}

QStringList ProFileEvaluator::Private::evaluateFunction(
        ProBlock *funcPtr, const QList<QStringList> &argumentsList, bool *ok)
{
    bool oki;
    QStringList ret;

    if (m_valuemapStack.count() >= 100) {
        errorMessage(format("ran into infinite recursion (depth > 100)."));
        oki = false;
    } else {
        State sts = m_sts;
        m_valuemapStack.push(m_valuemap);
        m_filevaluemapStack.push(m_filevaluemap);

        QStringList args;
        for (int i = 0; i < argumentsList.count(); ++i) {
            args += argumentsList[i];
            m_valuemap[QString::number(i+1)] = argumentsList[i];
        }
        m_valuemap[QLatin1String("ARGS")] = args;
        oki = (funcPtr->Accept(this) != ProItem::ReturnFalse); // True || Return
        ret = m_returnValue;
        m_returnValue.clear();

        m_valuemap = m_valuemapStack.pop();
        m_filevaluemap = m_filevaluemapStack.pop();
        m_sts = sts;
    }
    if (ok)
        *ok = oki;
    if (oki)
        return ret;
    return QStringList();
}

QStringList ProFileEvaluator::Private::evaluateExpandFunction(const QString &func, const QString &arguments)
{
    QList<QStringList> args_list = prepareFunctionArgs(arguments);

    if (ProBlock *funcPtr = m_functionDefs.replaceFunctions.value(func, 0))
        return evaluateFunction(funcPtr, args_list, 0);

    QStringList args; //why don't the builtin functions just use args_list? --Sam
    foreach (const QStringList &arg, args_list)
        args += arg.join(Option::field_sep);

    enum ExpandFunc { E_MEMBER=1, E_FIRST, E_LAST, E_CAT, E_FROMFILE, E_EVAL, E_LIST,
                      E_SPRINTF, E_JOIN, E_SPLIT, E_BASENAME, E_DIRNAME, E_SECTION,
                      E_FIND, E_SYSTEM, E_UNIQUE, E_QUOTE, E_ESCAPE_EXPAND,
                      E_UPPER, E_LOWER, E_FILES, E_PROMPT, E_RE_ESCAPE,
                      E_REPLACE };

    static QHash<QString, int> expands;
    if (expands.isEmpty()) {
        expands.insert(QLatin1String("member"), E_MEMBER);
        expands.insert(QLatin1String("first"), E_FIRST);
        expands.insert(QLatin1String("last"), E_LAST);
        expands.insert(QLatin1String("cat"), E_CAT);
        expands.insert(QLatin1String("fromfile"), E_FROMFILE);
        expands.insert(QLatin1String("eval"), E_EVAL);
        expands.insert(QLatin1String("list"), E_LIST);
        expands.insert(QLatin1String("sprintf"), E_SPRINTF);
        expands.insert(QLatin1String("join"), E_JOIN);
        expands.insert(QLatin1String("split"), E_SPLIT);
        expands.insert(QLatin1String("basename"), E_BASENAME);
        expands.insert(QLatin1String("dirname"), E_DIRNAME);
        expands.insert(QLatin1String("section"), E_SECTION);
        expands.insert(QLatin1String("find"), E_FIND);
        expands.insert(QLatin1String("system"), E_SYSTEM);
        expands.insert(QLatin1String("unique"), E_UNIQUE);
        expands.insert(QLatin1String("quote"), E_QUOTE);
        expands.insert(QLatin1String("escape_expand"), E_ESCAPE_EXPAND);
        expands.insert(QLatin1String("upper"), E_UPPER);
        expands.insert(QLatin1String("lower"), E_LOWER);
        expands.insert(QLatin1String("re_escape"), E_RE_ESCAPE);
        expands.insert(QLatin1String("files"), E_FILES);
        expands.insert(QLatin1String("prompt"), E_PROMPT); // interactive, so cannot be implemented
        expands.insert(QLatin1String("replace"), E_REPLACE);
    }
    ExpandFunc func_t = ExpandFunc(expands.value(func.toLower()));

    QStringList ret;

    switch (func_t) {
        case E_BASENAME:
        case E_DIRNAME:
        case E_SECTION: {
            bool regexp = false;
            QString sep, var;
            int beg = 0;
            int end = -1;
            if (func_t == E_SECTION) {
                if (args.count() != 3 && args.count() != 4) {
                    logMessage(format("%1(var) section(var, sep, begin, end) "
                        "requires three or four arguments.").arg(func));
                } else {
                    var = args[0];
                    sep = args[1];
                    beg = args[2].toInt();
                    if (args.count() == 4)
                        end = args[3].toInt();
                }
            } else {
                if (args.count() != 1) {
                    logMessage(format("%1(var) requires one argument.").arg(func));
                } else {
                    var = args[0];
                    regexp = true;
                    sep = QLatin1String("[\\\\/]");
                    if (func_t == E_DIRNAME)
                        end = -2;
                    else
                        beg = -1;
                }
            }
            if (!var.isNull()) {
                foreach (const QString str, values(var)) {
                    if (regexp)
                        ret += str.section(QRegExp(sep), beg, end);
                    else
                        ret += str.section(sep, beg, end);
                }
            }
            break;
        }
        case E_SPRINTF:
            if(args.count() < 1) {
                logMessage(format("sprintf(format, ...) requires at least one argument"));
            } else {
                QString tmp = args.at(0);
                for (int i = 1; i < args.count(); ++i)
                    tmp = tmp.arg(args.at(i));
                ret = split_value_list(tmp);
            }
            break;
        case E_JOIN: {
            if (args.count() < 1 || args.count() > 4) {
                logMessage(format("join(var, glue, before, after) requires one to four arguments."));
            } else {
                QString glue, before, after;
                if (args.count() >= 2)
                    glue = args[1];
                if (args.count() >= 3)
                    before = args[2];
                if (args.count() == 4)
                    after = args[3];
                const QStringList &var = values(args.first());
                if (!var.isEmpty())
                    ret.append(before + var.join(glue) + after);
            }
            break;
        }
        case E_SPLIT:
            if (args.count() != 2) {
                logMessage(format("split(var, sep) requires one or two arguments"));
            } else {
                const QString &sep = (args.count() == 2) ? args[1] : Option::field_sep;
                foreach (const QString &var, values(args.first()))
                    foreach (const QString &splt, var.split(sep))
                        ret.append(splt);
            }
            break;
        case E_MEMBER:
            if (args.count() < 1 || args.count() > 3) {
                logMessage(format("member(var, start, end) requires one to three arguments."));
            } else {
                bool ok = true;
                const QStringList var = values(args.first());
                int start = 0, end = 0;
                if (args.count() >= 2) {
                    QString start_str = args[1];
                    start = start_str.toInt(&ok);
                    if (!ok) {
                        if (args.count() == 2) {
                            int dotdot = start_str.indexOf(QLatin1String(".."));
                            if (dotdot != -1) {
                                start = start_str.left(dotdot).toInt(&ok);
                                if (ok)
                                    end = start_str.mid(dotdot+2).toInt(&ok);
                            }
                        }
                        if (!ok)
                            logMessage(format("member() argument 2 (start) '%2' invalid.")
                                .arg(start_str));
                    } else {
                        end = start;
                        if (args.count() == 3)
                            end = args[2].toInt(&ok);
                        if (!ok)
                            logMessage(format("member() argument 3 (end) '%2' invalid.\n")
                                .arg(args[2]));
                    }
                }
                if (ok) {
                    if (start < 0)
                        start += var.count();
                    if (end < 0)
                        end += var.count();
                    if (start < 0 || start >= var.count() || end < 0 || end >= var.count()) {
                        //nothing
                    } else if (start < end) {
                        for (int i = start; i <= end && var.count() >= i; i++)
                            ret.append(var[i]);
                    } else {
                        for (int i = start; i >= end && var.count() >= i && i >= 0; i--)
                            ret += var[i];
                    }
                }
            }
            break;
        case E_FIRST:
        case E_LAST:
            if (args.count() != 1) {
                logMessage(format("%1(var) requires one argument.").arg(func));
            } else {
                const QStringList var = values(args.first());
                if (!var.isEmpty()) {
                    if (func_t == E_FIRST)
                        ret.append(var[0]);
                    else
                        ret.append(var.last());
                }
            }
            break;
        case E_CAT:
            if (args.count() < 1 || args.count() > 2) {
                logMessage(format("cat(file, singleline=true) requires one or two arguments."));
            } else {
                QString file = args[0];

                bool singleLine = true;
                if (args.count() > 1)
                    singleLine = (!args[1].compare(QLatin1String("true"), Qt::CaseInsensitive));

                QFile qfile(file);
                if (qfile.open(QIODevice::ReadOnly)) {
                    QTextStream stream(&qfile);
                    while (!stream.atEnd()) {
                        ret += split_value_list(stream.readLine().trimmed());
                        if (!singleLine)
                            ret += QLatin1String("\n");
                    }
                    qfile.close();
                }
            }
            break;
        case E_FROMFILE:
            if (args.count() != 2) {
                logMessage(format("fromfile(file, variable) requires two arguments."));
            } else {
                QHash<QString, QStringList> vars;
                if (evaluateFileInto(args.at(0), &vars, 0))
                    ret = vars.value(args.at(1));
            }
            break;
        case E_EVAL:
            if (args.count() != 1) {
                logMessage(format("eval(variable) requires one argument"));
            } else {
                ret += values(args.at(0));
            }
            break;
        case E_LIST: {
            static int x = 0;
            QString tmp;
            tmp.sprintf(".QMAKE_INTERNAL_TMP_variableName_%d", x++);
            ret = QStringList(tmp);
            QStringList lst;
            foreach (const QString &arg, args)
                lst += split_value_list(arg);
            m_valuemap[tmp] = lst;
            break; }
        case E_FIND:
            if (args.count() != 2) {
                logMessage(format("find(var, str) requires two arguments."));
            } else {
                QRegExp regx(args[1]);
                foreach (const QString &val, values(args.first()))
                    if (regx.indexIn(val) != -1)
                        ret += val;
            }
            break;
        case E_SYSTEM:
            if (!m_skipLevel) {
                if (args.count() < 1 || args.count() > 2) {
                    logMessage(format("system(execute) requires one or two arguments."));
                } else {
                    char buff[256];
                    FILE *proc = QT_POPEN(args[0].toLatin1(), "r");
                    bool singleLine = true;
                    if (args.count() > 1)
                        singleLine = (!args[1].compare(QLatin1String("true"), Qt::CaseInsensitive));
                    QString output;
                    while (proc && !feof(proc)) {
                        int read_in = int(fread(buff, 1, 255, proc));
                        if (!read_in)
                            break;
                        for (int i = 0; i < read_in; i++) {
                            if ((singleLine && buff[i] == '\n') || buff[i] == '\t')
                                buff[i] = ' ';
                        }
                        buff[read_in] = '\0';
                        output += QLatin1String(buff);
                    }
                    ret += split_value_list(output);
                    if (proc)
                        QT_PCLOSE(proc);
                }
            }
            break;
        case E_UNIQUE:
            if(args.count() != 1) {
                logMessage(format("unique(var) requires one argument."));
            } else {
                foreach (const QString &var, values(args.first()))
                    if (!ret.contains(var))
                        ret.append(var);
            }
            break;
        case E_QUOTE:
            for (int i = 0; i < args.count(); ++i)
                ret += QStringList(args.at(i));
            break;
        case E_ESCAPE_EXPAND:
            for (int i = 0; i < args.size(); ++i) {
                QChar *i_data = args[i].data();
                int i_len = args[i].length();
                for (int x = 0; x < i_len; ++x) {
                    if (*(i_data+x) == QLatin1Char('\\') && x < i_len-1) {
                        if (*(i_data+x+1) == QLatin1Char('\\')) {
                            ++x;
                        } else {
                            struct {
                                char in, out;
                            } mapped_quotes[] = {
                                { 'n', '\n' },
                                { 't', '\t' },
                                { 'r', '\r' },
                                { 0, 0 }
                            };
                            for (int i = 0; mapped_quotes[i].in; ++i) {
                                if (*(i_data+x+1) == QLatin1Char(mapped_quotes[i].in)) {
                                    *(i_data+x) = QLatin1Char(mapped_quotes[i].out);
                                    if (x < i_len-2)
                                        memmove(i_data+x+1, i_data+x+2, (i_len-x-2)*sizeof(QChar));
                                    --i_len;
                                    break;
                                }
                            }
                        }
                    }
                }
                ret.append(QString(i_data, i_len));
            }
            break;
        case E_RE_ESCAPE:
            for (int i = 0; i < args.size(); ++i)
                ret += QRegExp::escape(args[i]);
            break;
        case E_UPPER:
        case E_LOWER:
            for (int i = 0; i < args.count(); ++i)
                if (func_t == E_UPPER)
                    ret += args[i].toUpper();
                else
                    ret += args[i].toLower();
            break;
        case E_FILES:
            if (args.count() != 1 && args.count() != 2) {
                logMessage(format("files(pattern, recursive=false) requires one or two arguments"));
            } else {
                bool recursive = false;
                if (args.count() == 2)
                    recursive = (!args[1].compare(QLatin1String("true"), Qt::CaseInsensitive) || args[1].toInt());
                QStringList dirs;
                QString r = fixPathToLocalOS(args[0]);
                int slash = r.lastIndexOf(QDir::separator());
                if (slash != -1) {
                    dirs.append(r.left(slash));
                    r = r.mid(slash+1);
                } else {
                    dirs.append(QString());
                }

                const QRegExp regex(r, Qt::CaseSensitive, QRegExp::Wildcard);
                for (int d = 0; d < dirs.count(); d++) {
                    QString dir = dirs[d];
                    if (!dir.isEmpty() && !dir.endsWith(m_option->dir_sep))
                        dir += QLatin1Char('/');

                    QDir qdir(dir);
                    for (int i = 0; i < (int)qdir.count(); ++i) {
                        if (qdir[i] == QLatin1String(".") || qdir[i] == QLatin1String(".."))
                            continue;
                        QString fname = dir + qdir[i];
                        if (QFileInfo(fname).isDir()) {
                            if (recursive)
                                dirs.append(fname);
                        }
                        if (regex.exactMatch(qdir[i]))
                            ret += fname;
                    }
                }
            }
            break;
        case E_REPLACE:
            if(args.count() != 3 ) {
                logMessage(format("replace(var, before, after) requires three arguments"));
            } else {
                const QRegExp before(args[1]);
                const QString after(args[2]);
                foreach (QString val, values(args.first()))
                    ret += val.replace(before, after);
            }
            break;
        case 0:
            logMessage(format("'%1' is not a recognized replace function").arg(func));
            break;
        default:
            logMessage(format("Function '%1' is not implemented").arg(func));
            break;
    }

    return ret;
}

ProItem::ProItemReturn ProFileEvaluator::Private::evaluateConditionalFunction(
        const QString &function, const QString &arguments)
{
    QList<QStringList> args_list = prepareFunctionArgs(arguments);

    if (ProBlock *funcPtr = m_functionDefs.testFunctions.value(function, 0)) {
        bool ok;
        QStringList ret = evaluateFunction(funcPtr, args_list, &ok);
        if (ok) {
            if (ret.isEmpty()) {
                return ProItem::ReturnTrue;
            } else {
                if (ret.first() != QLatin1String("false")) {
                    if (ret.first() == QLatin1String("true")) {
                        return ProItem::ReturnTrue;
                    } else {
                        bool ok;
                        int val = ret.first().toInt(&ok);
                        if (ok) {
                            if (val)
                                return ProItem::ReturnTrue;
                        } else {
                            logMessage(format("Unexpected return value from test '%1': %2")
                                          .arg(function).arg(ret.join(QLatin1String(" :: "))));
                        }
                    }
                }
            }
        }
        return ProItem::ReturnFalse;
    }

    QStringList args; //why don't the builtin functions just use args_list? --Sam
    foreach (const QStringList &arg, args_list)
        args += arg.join(Option::field_sep);

    enum TestFunc { T_REQUIRES=1, T_GREATERTHAN, T_LESSTHAN, T_EQUALS,
                    T_EXISTS, T_EXPORT, T_CLEAR, T_UNSET, T_EVAL, T_CONFIG, T_SYSTEM,
                    T_RETURN, T_BREAK, T_NEXT, T_DEFINED, T_CONTAINS, T_INFILE,
                    T_COUNT, T_ISEMPTY, T_INCLUDE, T_LOAD, T_DEBUG, T_MESSAGE, T_IF,
                    T_FOR, T_DEFINE_TEST, T_DEFINE_REPLACE };

    static QHash<QString, int> functions;
    if (functions.isEmpty()) {
        functions.insert(QLatin1String("requires"), T_REQUIRES);
        functions.insert(QLatin1String("greaterThan"), T_GREATERTHAN);
        functions.insert(QLatin1String("lessThan"), T_LESSTHAN);
        functions.insert(QLatin1String("equals"), T_EQUALS);
        functions.insert(QLatin1String("isEqual"), T_EQUALS);
        functions.insert(QLatin1String("exists"), T_EXISTS);
        functions.insert(QLatin1String("export"), T_EXPORT);
        functions.insert(QLatin1String("clear"), T_CLEAR);
        functions.insert(QLatin1String("unset"), T_UNSET);
        functions.insert(QLatin1String("eval"), T_EVAL);
        functions.insert(QLatin1String("CONFIG"), T_CONFIG);
        functions.insert(QLatin1String("if"), T_IF);
        functions.insert(QLatin1String("isActiveConfig"), T_CONFIG);
        functions.insert(QLatin1String("system"), T_SYSTEM);
        functions.insert(QLatin1String("return"), T_RETURN);
        functions.insert(QLatin1String("break"), T_BREAK);
        functions.insert(QLatin1String("next"), T_NEXT);
        functions.insert(QLatin1String("defined"), T_DEFINED);
        functions.insert(QLatin1String("contains"), T_CONTAINS);
        functions.insert(QLatin1String("infile"), T_INFILE);
        functions.insert(QLatin1String("count"), T_COUNT);
        functions.insert(QLatin1String("isEmpty"), T_ISEMPTY);
        functions.insert(QLatin1String("load"), T_LOAD);         //v
        functions.insert(QLatin1String("include"), T_INCLUDE);   //v
        functions.insert(QLatin1String("debug"), T_DEBUG);
        functions.insert(QLatin1String("message"), T_MESSAGE);   //v
        functions.insert(QLatin1String("warning"), T_MESSAGE);   //v
        functions.insert(QLatin1String("error"), T_MESSAGE);     //v
        functions.insert(QLatin1String("for"), T_FOR);     //v
        functions.insert(QLatin1String("defineTest"), T_DEFINE_TEST);        //v
        functions.insert(QLatin1String("defineReplace"), T_DEFINE_REPLACE);  //v
    }

    TestFunc func_t = (TestFunc)functions.value(function);

    switch (func_t) {
        case T_DEFINE_TEST:
            m_definingTest = true;
            goto defineFunc;
        case T_DEFINE_REPLACE:
            m_definingTest = false;
          defineFunc:
            if (args.count() != 1) {
                logMessage(format("%s(function) requires one argument.").arg(function));
                return ProItem::ReturnFalse;
            }
            m_definingFunc = args.first();
            return ProItem::ReturnTrue;
        case T_DEFINED:
            if (args.count() < 1 || args.count() > 2) {
                logMessage(format("defined(function, [\"test\"|\"replace\"])"
                                     " requires one or two arguments."));
                return ProItem::ReturnFalse;
            }
            if (args.count() > 1) {
                if (args[1] == QLatin1String("test"))
                    return returnBool(m_functionDefs.testFunctions.contains(args[0]));
                else if (args[1] == QLatin1String("replace"))
                    return returnBool(m_functionDefs.replaceFunctions.contains(args[0]));
                logMessage(format("defined(function, type):"
                                     " unexpected type [%1].\n").arg(args[1]));
                return ProItem::ReturnFalse;
            }
            return returnBool(m_functionDefs.replaceFunctions.contains(args[0])
                              || m_functionDefs.testFunctions.contains(args[0]));
        case T_RETURN:
            m_returnValue = args;
            // It is "safe" to ignore returns - due to qmake brokeness
            // they cannot be used to terminate loops anyway.
            if (m_skipLevel || m_cumulative)
                return ProItem::ReturnTrue;
            if (m_valuemapStack.isEmpty()) {
                logMessage(format("unexpected return()."));
                return ProItem::ReturnFalse;
            }
            return ProItem::ReturnReturn;
        case T_EXPORT:
            if (m_skipLevel && !m_cumulative)
                return ProItem::ReturnTrue;
            if (args.count() != 1) {
                logMessage(format("export(variable) requires one argument."));
                return ProItem::ReturnFalse;
            }
            for (int i = 0; i < m_valuemapStack.size(); ++i) {
                m_valuemapStack[i][args[0]] = m_valuemap.value(args[0]);
                m_filevaluemapStack[i][currentProFile()][args[0]] =
                        m_filevaluemap.value(currentProFile()).value(args[0]);
            }
            return ProItem::ReturnTrue;
        case T_INFILE:
            if (args.count() < 2 || args.count() > 3) {
                logMessage(format("infile(file, var, [values]) requires two or three arguments."));
            } else {
                QHash<QString, QStringList> vars;
                if (!evaluateFileInto(args.at(0), &vars, 0))
                    return ProItem::ReturnFalse;
                if (args.count() == 2)
                    return returnBool(vars.contains(args.at(1)));
                QRegExp regx(args.at(2));
                foreach (const QString &s, vars.value(args.at(1)))
                    if (s == regx.pattern() || regx.exactMatch(s))
                        return ProItem::ReturnTrue;
            }
            return ProItem::ReturnFalse;
#if 0
        case T_REQUIRES:
#endif
        case T_EVAL: {
                ProBlock *pro = new ProBlock(0);
                if (!read(pro, args.join(QLatin1String(" ")))) {
                    delete pro;
                    return ProItem::ReturnFalse;
                }
                bool ret = pro->Accept(this);
                pro->deref();
                return returnBool(ret);
            }
        case T_FOR: {
            if (m_cumulative) // This is a no-win situation, so just pretend it's no loop
                return ProItem::ReturnTrue;
            if (m_skipLevel)
                return ProItem::ReturnFalse;
            if (args.count() > 2 || args.count() < 1) {
                logMessage(format("for({var, list|var, forever|ever})"
                                     " requires one or two arguments."));
                return ProItem::ReturnFalse;
            }
            ProLoop loop;
            loop.infinite = false;
            loop.index = 0;
            QString it_list;
            if (args.count() == 1) {
                doVariableReplace(&args[0]);
                it_list = args[0];
                if (args[0] != QLatin1String("ever")) {
                    logMessage(format("for({var, list|var, forever|ever})"
                                         " requires one or two arguments."));
                    return ProItem::ReturnFalse;
                }
                it_list = QLatin1String("forever");
            } else {
                loop.variable = args[0];
                loop.oldVarVal = m_valuemap.value(loop.variable);
                doVariableReplace(&args[1]);
                it_list = args[1];
            }
            loop.list = m_valuemap.value(it_list);
            if (loop.list.isEmpty()) {
                if (it_list == QLatin1String("forever")) {
                    loop.infinite = true;
                } else {
                    int dotdot = it_list.indexOf(QLatin1String(".."));
                    if (dotdot != -1) {
                        bool ok;
                        int start = it_list.left(dotdot).toInt(&ok);
                        if (ok) {
                            int end = it_list.mid(dotdot+2).toInt(&ok);
                            if (ok) {
                                if (start < end) {
                                    for (int i = start; i <= end; i++)
                                        loop.list << QString::number(i);
                                } else {
                                    for (int i = start; i >= end; i--)
                                        loop.list << QString::number(i);
                                }
                            }
                        }
                    }
                }
            }
            m_loopStack.push(loop);
            m_sts.condition = true;
            return ProItem::ReturnLoop;
        }
        case T_BREAK:
            if (m_skipLevel)
                return ProItem::ReturnFalse;
            if (!m_loopStack.isEmpty())
                return ProItem::ReturnBreak;
            // ### missing: breaking out of multiline blocks
            logMessage(format("unexpected break()."));
            return ProItem::ReturnFalse;
        case T_NEXT:
            if (m_skipLevel)
                return ProItem::ReturnFalse;
            if (!m_loopStack.isEmpty())
                return ProItem::ReturnNext;
            logMessage(format("unexpected next()."));
            return ProItem::ReturnFalse;
        case T_IF: {
            if (args.count() != 1) {
                logMessage(format("if(condition) requires one argument."));
                return ProItem::ReturnFalse;
            }
            QString cond = args.first();
            bool escaped = false; // This is more than qmake does
            bool quoted = false;
            bool ret = true;
            bool orOp = false;
            bool invert = false;
            bool isFunc = false;
            int parens = 0;
            QString test;
            test.reserve(20);
            QString args;
            args.reserve(50);
            const QChar *d = cond.unicode();
            const QChar *ed = d + cond.length();
            while (d < ed) {
                ushort c = (d++)->unicode();
                if (!escaped) {
                    if (c == '\\') {
                        escaped = true;
                        args += c; // Assume no-one quotes the test name
                        continue;
                    } else if (c == '"') {
                        quoted = !quoted;
                        args += c; // Ditto
                        continue;
                    }
                } else {
                    escaped = false;
                }
                if (quoted) {
                    args += c; // Ditto
                } else {
                    bool isOp = false;
                    if (c == '(') {
                        isFunc = true;
                        if (parens)
                            args += c;
                        ++parens;
                    } else if (c == ')') {
                        --parens;
                        if (parens)
                            args += c;
                    } else if (!parens) {
                        if (c == ':' || c == '|')
                            isOp = true;
                        else if (c == '!')
                            invert = true;
                        else
                            test += c;
                    } else {
                        args += c;
                    }
                    if (!parens && (isOp || d == ed)) {
                        // Yes, qmake doesn't shortcut evaluations here. We can't, either,
                        // as some test functions have side effects.
                        bool success;
                        if (isFunc) {
                            success = evaluateConditionalFunction(test, args);
                        } else {
                            success = isActiveConfig(test, true);
                        }
                        success ^= invert;
                        if (orOp)
                            ret |= success;
                        else
                            ret &= success;
                        orOp = (c == '|');
                        invert = false;
                        isFunc = false;
                        test.clear();
                        args.clear();
                    }
                }
            }
            return returnBool(ret);
        }
        case T_CONFIG: {
            if (args.count() < 1 || args.count() > 2) {
                logMessage(format("CONFIG(config) requires one or two arguments."));
                return ProItem::ReturnFalse;
            }
            if (args.count() == 1) {
                //cond = isActiveConfig(args.first()); XXX
                return ProItem::ReturnFalse;
            }
            const QStringList mutuals = args[1].split(QLatin1Char('|'));
            const QStringList &configs = valuesDirect(QLatin1String("CONFIG"));

            for (int i = configs.size() - 1; i >= 0; i--) {
                for (int mut = 0; mut < mutuals.count(); mut++) {
                    if (configs[i] == mutuals[mut].trimmed()) {
                        return returnBool(configs[i] == args[0]);
                    }
                }
            }
            return ProItem::ReturnFalse;
        }
        case T_CONTAINS: {
            if (args.count() < 2 || args.count() > 3) {
                logMessage(format("contains(var, val) requires two or three arguments."));
                return ProItem::ReturnFalse;
            }

            QRegExp regx(args[1]);
            const QStringList &l = values(args.first());
            if (args.count() == 2) {
                for (int i = 0; i < l.size(); ++i) {
                    const QString val = l[i];
                    if (regx.exactMatch(val) || val == args[1]) {
                        return ProItem::ReturnTrue;
                    }
                }
            } else {
                const QStringList mutuals = args[2].split(QLatin1Char('|'));
                for (int i = l.size() - 1; i >= 0; i--) {
                    const QString val = l[i];
                    for (int mut = 0; mut < mutuals.count(); mut++) {
                        if (val == mutuals[mut].trimmed()) {
                            return returnBool(regx.exactMatch(val) || val == args[1]);
                        }
                    }
                }
            }
            return ProItem::ReturnFalse;
        }
        case T_COUNT:
            if (args.count() != 2 && args.count() != 3) {
                logMessage(format("count(var, count, op=\"equals\") requires two or three arguments."));
                return ProItem::ReturnFalse;
            }
            if (args.count() == 3) {
                QString comp = args[2];
                if (comp == QLatin1String(">") || comp == QLatin1String("greaterThan")) {
                    return returnBool(values(args.first()).count() > args[1].toInt());
                } else if (comp == QLatin1String(">=")) {
                    return returnBool(values(args.first()).count() >= args[1].toInt());
                } else if (comp == QLatin1String("<") || comp == QLatin1String("lessThan")) {
                    return returnBool(values(args.first()).count() < args[1].toInt());
                } else if (comp == QLatin1String("<=")) {
                    return returnBool(values(args.first()).count() <= args[1].toInt());
                } else if (comp == QLatin1String("equals") || comp == QLatin1String("isEqual")
                           || comp == QLatin1String("=") || comp == QLatin1String("==")) {
                    return returnBool(values(args.first()).count() == args[1].toInt());
                } else {
                    logMessage(format("unexpected modifier to count(%2)").arg(comp));
                    return ProItem::ReturnFalse;
                }
            }
            return returnBool(values(args.first()).count() == args[1].toInt());
        case T_GREATERTHAN:
        case T_LESSTHAN: {
            if (args.count() != 2) {
                logMessage(format("%1(variable, value) requires two arguments.").arg(function));
                return ProItem::ReturnFalse;
            }
            QString rhs(args[1]), lhs(values(args[0]).join(Option::field_sep));
            bool ok;
            int rhs_int = rhs.toInt(&ok);
            if (ok) { // do integer compare
                int lhs_int = lhs.toInt(&ok);
                if (ok) {
                    if (func_t == T_GREATERTHAN)
                        return returnBool(lhs_int > rhs_int);
                    return returnBool(lhs_int < rhs_int);
                }
            }
            if (func_t == T_GREATERTHAN)
                return returnBool(lhs > rhs);
            return returnBool(lhs < rhs);
        }
        case T_EQUALS:
            if (args.count() != 2) {
                logMessage(format("%1(variable, value) requires two arguments.").arg(function));
                return ProItem::ReturnFalse;
            }
            return returnBool(values(args[0]).join(Option::field_sep) == args[1]);
        case T_CLEAR: {
            if (m_skipLevel && !m_cumulative)
                return ProItem::ReturnFalse;
            if (args.count() != 1) {
                logMessage(format("%1(variable) requires one argument.").arg(function));
                return ProItem::ReturnFalse;
            }
            QHash<QString, QStringList>::Iterator it = m_valuemap.find(args[0]);
            if (it == m_valuemap.end())
                return ProItem::ReturnFalse;
            it->clear();
            return ProItem::ReturnTrue;
        }
        case T_UNSET: {
            if (m_skipLevel && !m_cumulative)
                return ProItem::ReturnFalse;
            if (args.count() != 1) {
                logMessage(format("%1(variable) requires one argument.").arg(function));
                return ProItem::ReturnFalse;
            }
            QHash<QString, QStringList>::Iterator it = m_valuemap.find(args[0]);
            if (it == m_valuemap.end())
                return ProItem::ReturnFalse;
            m_valuemap.erase(it);
            return ProItem::ReturnTrue;
        }
        case T_INCLUDE: {
            if (m_skipLevel && !m_cumulative)
                return ProItem::ReturnFalse;
            QString parseInto;
            // the third optional argument to include() controls warnings
            //      and is not used here
            if ((args.count() == 2) || (args.count() == 3) ) {
                parseInto = args[1];
            } else if (args.count() != 1) {
                logMessage(format("include(file) requires one, two or three arguments."));
                return ProItem::ReturnFalse;
            }
            QString fileName = args.first();
            // ### this breaks if we have include(c:/reallystupid.pri) but IMHO that's really bad style.
            QDir currentProPath(currentDirectory());
            fileName = QDir::cleanPath(currentProPath.absoluteFilePath(fileName));
            State sts = m_sts;
            bool ok = evaluateFile(fileName);
            m_sts = sts;
            return returnBool(ok);
        }
        case T_LOAD: {
            if (m_skipLevel && !m_cumulative)
                return ProItem::ReturnFalse;
            QString parseInto;
            bool ignore_error = false;
            if (args.count() == 2) {
                QString sarg = args[1];
                ignore_error = (!sarg.compare(QLatin1String("true"), Qt::CaseInsensitive) || sarg.toInt());
            } else if (args.count() != 1) {
                logMessage(format("load(feature) requires one or two arguments."));
                return ProItem::ReturnFalse;
            }
            // XXX ignore_error unused
            return returnBool(evaluateFeatureFile(args.first()));
        }
        case T_DEBUG:
            // Yup - do nothing. Nothing is going to enable debug output anyway.
            return ProItem::ReturnFalse;
        case T_MESSAGE: {
            if (args.count() != 1) {
                logMessage(format("%1(message) requires one argument.").arg(function));
                return ProItem::ReturnFalse;
            }
            QString msg = expandEnvVars(args.first());
            fileMessage(QString::fromLatin1("Project %1: %2").arg(function.toUpper(), msg));
            // ### Consider real termination in non-cumulative mode
            return returnBool(function != QLatin1String("error"));
        }
#if 0 // Way too dangerous to enable.
        case T_SYSTEM: {
            if (args.count() != 1) {
                logMessage(format("system(exec) requires one argument."));
                ProItem::ReturnFalse;
            }
            return returnBool(system(args.first().toLatin1().constData()) == 0);
        }
#endif
        case T_ISEMPTY: {
            if (args.count() != 1) {
                logMessage(format("isEmpty(var) requires one argument."));
                return ProItem::ReturnFalse;
            }
            QStringList sl = values(args.first());
            if (sl.count() == 0) {
                return ProItem::ReturnTrue;
            } else if (sl.count() > 0) {
                QString var = sl.first();
                if (var.isEmpty())
                    return ProItem::ReturnTrue;
            }
            return ProItem::ReturnFalse;
        }
        case T_EXISTS: {
            if (args.count() != 1) {
                logMessage(format("exists(file) requires one argument."));
                return ProItem::ReturnFalse;
            }
            QString file = args.first();
            file = fixPathToLocalOS(file);

            if (QFile::exists(file)) {
                return ProItem::ReturnTrue;
            }
            //regular expression I guess
            QString dirstr = currentDirectory();
            int slsh = file.lastIndexOf(m_option->dir_sep);
            if (slsh != -1) {
                dirstr = file.left(slsh+1);
                file = file.right(file.length() - slsh - 1);
            }
            if (file.contains(QLatin1Char('*')) || file.contains(QLatin1Char('?')))
                if (!QDir(dirstr).entryList(QStringList(file)).isEmpty())
                    return ProItem::ReturnTrue;

            return ProItem::ReturnFalse;
        }
        case 0:
            logMessage(format("'%1' is not a recognized test function").arg(function));
            return ProItem::ReturnFalse;
        default:
            logMessage(format("Function '%1' is not implemented").arg(function));
            return ProItem::ReturnFalse;
    }
}

QStringList ProFileEvaluator::Private::values(const QString &variableName,
                                              const QHash<QString, QStringList> &place,
                                              const ProFile *pro) const
{
    if (variableName == QLatin1String("LITERAL_WHITESPACE")) //a real space in a token
        return QStringList(QLatin1String("\t"));
    if (variableName == QLatin1String("LITERAL_DOLLAR")) //a real $
        return QStringList(QLatin1String("$"));
    if (variableName == QLatin1String("LITERAL_HASH")) //a real #
        return QStringList(QLatin1String("#"));
    if (variableName == QLatin1String("OUT_PWD")) //the out going dir
        return QStringList(m_outputDir);
    if (variableName == QLatin1String("PWD") ||  //current working dir (of _FILE_)
        variableName == QLatin1String("IN_PWD"))
        return QStringList(currentDirectory());
    if (variableName == QLatin1String("DIR_SEPARATOR"))
        return QStringList(m_option->dir_sep);
    if (variableName == QLatin1String("DIRLIST_SEPARATOR"))
        return QStringList(m_option->dirlist_sep);
    if (variableName == QLatin1String("_LINE_")) //parser line number
        return QStringList(QString::number(m_lineNo));
    if (variableName == QLatin1String("_FILE_")) //parser file; qmake is a bit weird here
        return QStringList(m_profileStack.size() == 1 ? pro->fileName() : QFileInfo(pro->fileName()).fileName());
    if (variableName == QLatin1String("_DATE_")) //current date/time
        return QStringList(QDateTime::currentDateTime().toString());
    if (variableName == QLatin1String("_PRO_FILE_"))
        return QStringList(m_profileStack.first()->fileName());
    if (variableName == QLatin1String("_PRO_FILE_PWD_"))
        return  QStringList(QFileInfo(m_profileStack.first()->fileName()).absolutePath());
    if (variableName == QLatin1String("_QMAKE_CACHE_"))
        return QStringList(m_option->cachefile);
    if (variableName.startsWith(QLatin1String("QMAKE_HOST."))) {
        QString ret, type = variableName.mid(11);
#if defined(Q_OS_WIN32)
        if (type == QLatin1String("os")) {
            ret = QLatin1String("Windows");
        } else if (type == QLatin1String("name")) {
            DWORD name_length = 1024;
            TCHAR name[1024];
            if (GetComputerName(name, &name_length))
                ret = QString::fromUtf16((ushort*)name, name_length);
        } else if (type == QLatin1String("version") || type == QLatin1String("version_string")) {
            QSysInfo::WinVersion ver = QSysInfo::WindowsVersion;
            if (type == QLatin1String("version"))
                ret = QString::number(ver);
            else if (ver == QSysInfo::WV_Me)
                ret = QLatin1String("WinMe");
            else if (ver == QSysInfo::WV_95)
                ret = QLatin1String("Win95");
            else if (ver == QSysInfo::WV_98)
                ret = QLatin1String("Win98");
            else if (ver == QSysInfo::WV_NT)
                ret = QLatin1String("WinNT");
            else if (ver == QSysInfo::WV_2000)
                ret = QLatin1String("Win2000");
            else if (ver == QSysInfo::WV_2000)
                ret = QLatin1String("Win2003");
            else if (ver == QSysInfo::WV_XP)
                ret = QLatin1String("WinXP");
            else if (ver == QSysInfo::WV_VISTA)
                ret = QLatin1String("WinVista");
            else
                ret = QLatin1String("Unknown");
        } else if (type == QLatin1String("arch")) {
            SYSTEM_INFO info;
            GetSystemInfo(&info);
            switch(info.wProcessorArchitecture) {
#ifdef PROCESSOR_ARCHITECTURE_AMD64
            case PROCESSOR_ARCHITECTURE_AMD64:
                ret = QLatin1String("x86_64");
                break;
#endif
            case PROCESSOR_ARCHITECTURE_INTEL:
                ret = QLatin1String("x86");
                break;
            case PROCESSOR_ARCHITECTURE_IA64:
#ifdef PROCESSOR_ARCHITECTURE_IA32_ON_WIN64
            case PROCESSOR_ARCHITECTURE_IA32_ON_WIN64:
#endif
                ret = QLatin1String("IA64");
                break;
            default:
                ret = QLatin1String("Unknown");
                break;
            }
        }
#elif defined(Q_OS_UNIX)
        struct utsname name;
        if (!uname(&name)) {
            if (type == QLatin1String("os"))
                ret = QString::fromLatin1(name.sysname);
            else if (type == QLatin1String("name"))
                ret = QString::fromLatin1(name.nodename);
            else if (type == QLatin1String("version"))
                ret = QString::fromLatin1(name.release);
            else if (type == QLatin1String("version_string"))
                ret = QString::fromLatin1(name.version);
            else if (type == QLatin1String("arch"))
                ret = QString::fromLatin1(name.machine);
        }
#endif
        return QStringList(ret);
    }

    QStringList result = place.value(variableName);
    if (result.isEmpty()) {
        if (variableName == QLatin1String("TEMPLATE")) {
            result.append(QLatin1String("app"));
        } else if (variableName == QLatin1String("QMAKE_DIR_SEP")) {
            result.append(m_option->dirlist_sep);
        }
    }
    return result;
}

QStringList ProFileEvaluator::Private::values(const QString &variableName) const
{
    return values(variableName, m_valuemap, currentProFile());
}

QStringList ProFileEvaluator::Private::values(const QString &variableName, const ProFile *pro) const
{
    return values(variableName, m_filevaluemap[pro], pro);
}

// virtual
ProFile *ProFileEvaluator::parsedProFile(const QString &fileName)
{
    ProFile *pro = new ProFile(fileName);
    if (d->read(pro))
        return pro;
    delete pro;
    return 0;
}

// virtual
void ProFileEvaluator::releaseParsedProFile(ProFile *proFile)
{
    delete proFile;
}

bool ProFileEvaluator::Private::evaluateFile(const QString &fileName)
{
    QFileInfo fi(fileName);
    if (!fi.exists())
        return false;
    QString fn = QDir::cleanPath(fi.absoluteFilePath());
    foreach (const ProFile *pf, m_profileStack)
        if (pf->fileName() == fn) {
            errorMessage(format("circular inclusion of %1").arg(fn));
            return false;
        }
    ProFile *pro = q->parsedProFile(fn);
    if (pro) {
        bool ok = (pro->Accept(this) == ProItem::ReturnTrue);
        q->releaseParsedProFile(pro);
        return ok;
    } else {
        return false;
    }
}

bool ProFileEvaluator::Private::evaluateFeatureFile(
        const QString &fileName, QHash<QString, QStringList> *values, FunctionDefs *funcs)
{
    QString fn = fileName;
    if (!fn.endsWith(QLatin1String(".prf")))
        fn += QLatin1String(".prf");

    if (!fileName.contains((ushort)'/') || !QFile::exists(fn)) {
        if (m_option->feature_roots.isEmpty())
            m_option->feature_roots = qmakeFeaturePaths();
        int start_root = 0;
        QString currFn = currentFileName();
        if (QFileInfo(currFn).fileName() == QFileInfo(fn).fileName()) {
            for (int root = 0; root < m_option->feature_roots.size(); ++root)
                if (m_option->feature_roots.at(root) + fn == currFn) {
                    start_root = root + 1;
                    break;
                }
        }
        for (int root = start_root; root < m_option->feature_roots.size(); ++root) {
            QString fname = m_option->feature_roots.at(root) + fn;
            if (QFileInfo(fname).exists()) {
                fn = fname;
                goto cool;
            }
        }
        return false;

      cool:
        // It's beyond me why qmake has this inside this if ...
        QStringList &already = m_valuemap[QLatin1String("QMAKE_INTERNAL_INCLUDED_FEATURES")];
        if (already.contains(fn))
            return true;
        already.append(fn);
    } else {
        fn = QDir::cleanPath(fn);
    }

    if (values) {
        return evaluateFileInto(fn, values, funcs);
    } else {
        bool cumulative = m_cumulative;
        m_cumulative = false;

        // Don't use evaluateFile() here to avoid the virtual parsedProFile().
        // The path is fully normalized already.
        ProFile pro(fn);
        bool ok = false;
        if (read(&pro))
            ok = (pro.Accept(this) == ProItem::ReturnTrue);

        m_cumulative = cumulative;
        return ok;
    }
}

bool ProFileEvaluator::Private::evaluateFileInto(
        const QString &fileName, QHash<QString, QStringList> *values, FunctionDefs *funcs)
{
    ProFileEvaluator visitor(m_option);
    visitor.d->m_cumulative = false;
    visitor.d->m_parsePreAndPostFiles = false;
    visitor.d->m_verbose = m_verbose;
    visitor.d->m_valuemap = *values;
    if (funcs)
        visitor.d->m_functionDefs = *funcs;
    if (!visitor.d->evaluateFile(fileName))
        return false;
    *values = visitor.d->m_valuemap;
    if (funcs) {
        *funcs = visitor.d->m_functionDefs;
        // So they are not unref'd
        visitor.d->m_functionDefs.testFunctions.clear();
        visitor.d->m_functionDefs.replaceFunctions.clear();
    }
    return true;
}

QString ProFileEvaluator::Private::format(const char *fmt) const
{
    ProFile *pro = currentProFile();
    QString fileName = pro ? pro->fileName() : QLatin1String("Not a file");
    int lineNumber = pro ? m_lineNo : 0;
    return QString::fromLatin1("%1(%2):").arg(fileName).arg(lineNumber) + QString::fromAscii(fmt);
}

void ProFileEvaluator::Private::logMessage(const QString &message) const
{
    if (m_verbose && !m_skipLevel)
        q->logMessage(message);
}

void ProFileEvaluator::Private::fileMessage(const QString &message) const
{
    if (!m_skipLevel)
        q->fileMessage(message);
}

void ProFileEvaluator::Private::errorMessage(const QString &message) const
{
    if (!m_skipLevel)
        q->errorMessage(message);
}


///////////////////////////////////////////////////////////////////////
//
// ProFileEvaluator
//
///////////////////////////////////////////////////////////////////////

ProFileEvaluator::ProFileEvaluator(ProFileEvaluator::Option *option)
  : d(new Private(this, option))
{
}

ProFileEvaluator::~ProFileEvaluator()
{
    delete d;
}

bool ProFileEvaluator::contains(const QString &variableName) const
{
    return d->m_valuemap.contains(variableName);
}

QStringList ProFileEvaluator::values(const QString &variableName) const
{
    return expandEnvVars(d->values(variableName));
}

QStringList ProFileEvaluator::values(const QString &variableName, const ProFile *pro) const
{
    return expandEnvVars(d->values(variableName, pro));
}

QStringList ProFileEvaluator::absolutePathValues(
        const QString &variable, const QString &baseDirectory) const
{
    QStringList result;
    foreach (const QString &el, values(variable)) {
        const QFileInfo info = QFileInfo(baseDirectory, el);
        if (info.isDir())
            result << QDir::cleanPath(info.absoluteFilePath());
    }
    return result;
}

QStringList ProFileEvaluator::absoluteFileValues(
        const QString &variable, const QString &baseDirectory, const QStringList &searchDirs,
        const ProFile *pro) const
{
    QStringList result;
    foreach (const QString &el, pro ? values(variable, pro) : values(variable)) {
        QFileInfo info(el);
        if (info.isAbsolute()) {
            if (info.exists()) {
                result << QDir::cleanPath(el);
                goto next;
            }
        } else {
            foreach (const QString &dir, searchDirs) {
                QFileInfo info(dir, el);
                if (info.isFile()) {
                    result << QDir::cleanPath(info.filePath());
                    goto next;
                }
            }
            if (baseDirectory.isEmpty())
                goto next;
            info = QFileInfo(baseDirectory, el);
        }
        {
            QFileInfo baseInfo(info.absolutePath());
            if (baseInfo.exists()) {
                QString wildcard = info.fileName();
                if (wildcard.contains(QLatin1Char('*')) || wildcard.contains(QLatin1Char('?'))) {
                    QDir theDir(QDir::cleanPath(baseInfo.filePath()));
                    foreach (const QString &fn, theDir.entryList(QStringList(wildcard)))
                        if (fn != QLatin1String(".") && fn != QLatin1String(".."))
                            result << theDir.absoluteFilePath(fn);
                } // else if (acceptMissing)
            }
        }
      next: ;
    }
    return result;
}

ProFileEvaluator::TemplateType ProFileEvaluator::templateType()
{
    QStringList templ = values(QLatin1String("TEMPLATE"));
    if (templ.count() >= 1) {
        const QString &t = templ.last();
        if (!t.compare(QLatin1String("app"), Qt::CaseInsensitive))
            return TT_Application;
        if (!t.compare(QLatin1String("lib"), Qt::CaseInsensitive))
            return TT_Library;
        if (!t.compare(QLatin1String("script"), Qt::CaseInsensitive))
            return TT_Script;
        if (!t.compare(QLatin1String("subdirs"), Qt::CaseInsensitive))
            return TT_Subdirs;
    }
    return TT_Unknown;
}

bool ProFileEvaluator::queryProFile(ProFile *pro)
{
    return d->read(pro);
}

bool ProFileEvaluator::queryProFile(ProFile *pro, const QString &content)
{
    return d->read(pro, content);
}

bool ProFileEvaluator::accept(ProFile *pro)
{
    return pro->Accept(d);
}

QString ProFileEvaluator::propertyValue(const QString &name) const
{
    return d->propertyValue(name);
}

void ProFileEvaluator::logMessage(const QString &message)
{
    qWarning("%s", qPrintable(message));
}

void ProFileEvaluator::fileMessage(const QString &message)
{
    qWarning("%s", qPrintable(message));
}

void ProFileEvaluator::errorMessage(const QString &message)
{
    qWarning("%s", qPrintable(message));
}

void ProFileEvaluator::setVerbose(bool on)
{
    d->m_verbose = on;
}

void ProFileEvaluator::setCumulative(bool on)
{
    d->m_cumulative = on;
}

void ProFileEvaluator::setOutputDir(const QString &dir)
{
    d->m_outputDir = dir;
}

void ProFileEvaluator::setUserConfigCmdArgs(const QStringList &addUserConfigCmdArgs, const QStringList &removeUserConfigCmdArgs)
{
    d->m_addUserConfigCmdArgs = addUserConfigCmdArgs;
    d->m_removeUserConfigCmdArgs = removeUserConfigCmdArgs;
}

void ProFileEvaluator::setParsePreAndPostFiles(bool on)
{
    d->m_parsePreAndPostFiles = on;
}

QT_END_NAMESPACE
