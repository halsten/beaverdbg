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

#include "ResolveExpression.h"
#include "LookupContext.h"
#include "Overview.h"
#include "GenTemplateInstance.h"

#include <Control.h>
#include <AST.h>
#include <Scope.h>
#include <Names.h>
#include <Symbols.h>
#include <Literals.h>
#include <CoreTypes.h>
#include <TypeVisitor.h>
#include <NameVisitor.h>

#include <QtCore/QList>
#include <QtCore/QVarLengthArray>
#include <QtCore/QtDebug>

using namespace CPlusPlus;

namespace {

template <typename _Tp>
static QList<_Tp> removeDuplicates(const QList<_Tp> &results)
{
    QList<_Tp> uniqueList;
    QSet<_Tp> processed;
    foreach (const _Tp &r, results) {
        if (processed.contains(r))
            continue;

        processed.insert(r);
        uniqueList.append(r);
    }

    return uniqueList;
}

} // end of anonymous namespace

/////////////////////////////////////////////////////////////////////
// ResolveExpression
/////////////////////////////////////////////////////////////////////
ResolveExpression::ResolveExpression(const LookupContext &context)
    : ASTVisitor(context.expressionDocument()->control()),
      _context(context),
      sem(_context.control())
{ }

ResolveExpression::~ResolveExpression()
{ }

QList<ResolveExpression::Result> ResolveExpression::operator()(ExpressionAST *ast)
{
    const QList<Result> previousResults = switchResults(QList<Result>());
    accept(ast);
    return removeDuplicates(switchResults(previousResults));
}

QList<ResolveExpression::Result>
ResolveExpression::switchResults(const QList<ResolveExpression::Result> &results)
{
    const QList<Result> previousResults = _results;
    _results = results;
    return previousResults;
}

void ResolveExpression::addResults(const QList<Result> &results)
{
    foreach (const Result r, results)
        addResult(r);
}

void ResolveExpression::addResult(const FullySpecifiedType &ty, Symbol *symbol)
{ return addResult(Result(ty, symbol)); }

void ResolveExpression::addResult(const Result &r)
{
    Result p = r;

    if (! p.second)
        p.second = _context.symbol();

    if (! _results.contains(p))
        _results.append(p);
}

QList<Scope *> ResolveExpression::visibleScopes(const Result &result) const
{ return _context.visibleScopes(result); }

bool ResolveExpression::visit(ExpressionListAST *)
{
    // nothing to do.
    return false;
}

bool ResolveExpression::visit(BinaryExpressionAST *ast)
{
    accept(ast->left_expression);
    return false;
}

bool ResolveExpression::visit(CastExpressionAST *ast)
{
    addResult(sem.check(ast->type_id, _context.expressionDocument()->globalSymbols()));
    return false;
}

bool ResolveExpression::visit(ConditionAST *)
{
    // nothing to do.
    return false;
}

bool ResolveExpression::visit(ConditionalExpressionAST *ast)
{
    if (ast->left_expression)
        accept(ast->left_expression);

    else if (ast->right_expression)
        accept(ast->right_expression);

    return false;
}

bool ResolveExpression::visit(CppCastExpressionAST *ast)
{
    addResult(sem.check(ast->type_id, _context.expressionDocument()->globalSymbols()));
    return false;
}

bool ResolveExpression::visit(DeleteExpressionAST *)
{
    FullySpecifiedType ty(control()->voidType());
    addResult(ty);
    return false;
}

bool ResolveExpression::visit(ArrayInitializerAST *)
{
    // nothing to do.
    return false;
}

bool ResolveExpression::visit(NewExpressionAST *ast)
{
    if (ast->new_type_id) {
        Scope *scope = _context.expressionDocument()->globalSymbols();
        FullySpecifiedType ty = sem.check(ast->new_type_id->type_specifier, scope);
        ty = sem.check(ast->new_type_id->ptr_operators, ty, scope);
        FullySpecifiedType ptrTy(control()->pointerType(ty));
        addResult(ptrTy);
    }
    // nothing to do.
    return false;
}

bool ResolveExpression::visit(TypeidExpressionAST *)
{
    Name *std_type_info[2];
    std_type_info[0] = control()->nameId(control()->findOrInsertIdentifier("std"));
    std_type_info[1] = control()->nameId(control()->findOrInsertIdentifier("type_info"));

    Name *q = control()->qualifiedNameId(std_type_info, 2, /*global=*/ true);
    FullySpecifiedType ty(control()->namedType(q));
    addResult(ty);

    return false;
}

bool ResolveExpression::visit(TypenameCallExpressionAST *)
{
    // nothing to do
    return false;
}

bool ResolveExpression::visit(TypeConstructorCallAST *)
{
    // nothing to do.
    return false;
}

bool ResolveExpression::visit(PostfixExpressionAST *ast)
{
    accept(ast->base_expression);

    for (PostfixAST *fx = ast->postfix_expressions; fx; fx = fx->next) {
        accept(fx);
    }

    return false;
}

bool ResolveExpression::visit(SizeofExpressionAST *)
{
    FullySpecifiedType ty(control()->integerType(IntegerType::Int));
    ty.setUnsigned(true);
    addResult(ty);
    return false;
}

bool ResolveExpression::visit(NumericLiteralAST *ast)
{
    Type *type = 0;
    NumericLiteral *literal = numericLiteral(ast->literal_token);

    if (literal->isChar())
        type = control()->integerType(IntegerType::Char);
    else if (literal->isWideChar())
        type = control()->integerType(IntegerType::WideChar);
    else if (literal->isInt())
        type = control()->integerType(IntegerType::Int);
    else if (literal->isLong())
        type = control()->integerType(IntegerType::Long);
    else if (literal->isLongLong())
        type = control()->integerType(IntegerType::LongLong);
    else if (literal->isFloat())
        type = control()->floatType(FloatType::Float);
    else if (literal->isDouble())
        type = control()->floatType(FloatType::Double);
    else if (literal->isLongDouble())
        type = control()->floatType(FloatType::LongDouble);
    else
        type = control()->integerType(IntegerType::Int);

    FullySpecifiedType ty(type);
    if (literal->isUnsigned())
        ty.setUnsigned(true);

    addResult(ty);
    return false;
}

bool ResolveExpression::visit(BoolLiteralAST *)
{
    FullySpecifiedType ty(control()->integerType(IntegerType::Bool));
    addResult(ty);
    return false;
}

bool ResolveExpression::visit(ThisExpressionAST *)
{
    if (! _context.symbol())
        return false;

    Scope *scope = _context.symbol()->scope();
    for (; scope; scope = scope->enclosingScope()) {
        if (scope->isFunctionScope()) {
            Function *fun = scope->owner()->asFunction();
            if (Scope *cscope = scope->enclosingClassScope()) {
                Class *klass = cscope->owner()->asClass();
                FullySpecifiedType classTy(control()->namedType(klass->name()));
                FullySpecifiedType ptrTy(control()->pointerType(classTy));
                addResult(ptrTy, fun);
                break;
            } else if (QualifiedNameId *q = fun->name()->asQualifiedNameId()) {
                Name *nestedNameSpecifier = 0;
                if (q->nameCount() == 1 && q->isGlobal())
                    nestedNameSpecifier = q->nameAt(0);
                else
                    nestedNameSpecifier = control()->qualifiedNameId(q->names(), q->nameCount() - 1);
                FullySpecifiedType classTy(control()->namedType(nestedNameSpecifier));
                FullySpecifiedType ptrTy(control()->pointerType(classTy));
                addResult(ptrTy, fun);
                break;
            }
        }
    }
    return false;
}

bool ResolveExpression::visit(NestedExpressionAST *ast)
{
    accept(ast->expression);
    return false;
}

bool ResolveExpression::visit(StringLiteralAST *)
{
    FullySpecifiedType charTy = control()->integerType(IntegerType::Char);
    charTy.setConst(true);
    FullySpecifiedType ty(control()->pointerType(charTy));
    addResult(ty);
    return false;
}

bool ResolveExpression::visit(ThrowExpressionAST *)
{
    return false;
}

bool ResolveExpression::visit(TypeIdAST *)
{
    return false;
}

bool ResolveExpression::visit(UnaryExpressionAST *ast)
{
    accept(ast->expression);
    unsigned unaryOp = tokenKind(ast->unary_op_token);
    if (unaryOp == T_AMPER) {
        QMutableListIterator<Result > it(_results);
        while (it.hasNext()) {
            Result p = it.next();
            p.first.setType(control()->pointerType(p.first));
            it.setValue(p);
        }
    } else if (unaryOp == T_STAR) {
        QMutableListIterator<Result > it(_results);
        while (it.hasNext()) {
            Result p = it.next();
            if (PointerType *ptrTy = p.first->asPointerType()) {
                p.first = ptrTy->elementType();
                it.setValue(p);
            } else {
                it.remove();
            }
        }
    }
    return false;
}

bool ResolveExpression::visit(CompoundLiteralAST *ast)
{
    accept(ast->type_id);
    return false;
}

bool ResolveExpression::visit(QualifiedNameAST *ast)
{
    ResolveClass resolveClass;
    Name *name = ast->name;

    QList<Symbol *> symbols = _context.resolve(name);
    foreach (Symbol *symbol, symbols) {
        if (symbol->isTypedef()) {
            if (NamedType *namedTy = symbol->type()->asNamedType()) {
                const Result r(namedTy, symbol);
                const QList<Symbol *> resolvedClasses =
                        resolveClass(namedTy->name(), r, _context);
                if (resolvedClasses.count()) {
                    foreach (Symbol *s, resolvedClasses) {
                        addResult(s->type(), s);
                    }
                    continue;
                }
            }
        }
        addResult(symbol->type(), symbol);
    }
    return false;
}

bool ResolveExpression::visit(OperatorFunctionIdAST *)
{
    return false;
}

bool ResolveExpression::visit(ConversionFunctionIdAST *)
{
    return false;
}

bool ResolveExpression::visit(SimpleNameAST *ast)
{
    QList<Symbol *> symbols = _context.resolve(ast->name);
    foreach (Symbol *symbol, symbols)
        addResult(symbol->type(), symbol);

    return false;
}

bool ResolveExpression::visit(DestructorNameAST *)
{
    FullySpecifiedType ty(control()->voidType());
    addResult(ty);
    return false;
}

bool ResolveExpression::visit(TemplateIdAST *ast)
{
    QList<Symbol *> symbols = _context.resolve(ast->name);
    foreach (Symbol *symbol, symbols)
        addResult(symbol->type(), symbol);

    return false;
}

bool ResolveExpression::maybeValidPrototype(Function *funTy, unsigned actualArgumentCount) const
{
    unsigned minNumberArguments = 0;

    for (; minNumberArguments < funTy->argumentCount(); ++minNumberArguments) {
        Argument *arg = funTy->argumentAt(minNumberArguments)->asArgument();

        if (arg->hasInitializer())
            break;
    }

    if (actualArgumentCount < minNumberArguments) {
        // not enough arguments.
        return false;

    } else if (! funTy->isVariadic() && actualArgumentCount > funTy->argumentCount()) {
        // too many arguments.
        return false;
    }

    return true;
}

bool ResolveExpression::visit(CallAST *ast)
{
    ResolveClass resolveClass;

    const QList<Result> baseResults = _results;
    _results.clear();

    // Compute the types of the actual arguments.
    int actualArgumentCount = 0;

    //QList< QList<Result> > arguments;
    for (ExpressionListAST *exprIt = ast->expression_list; exprIt; exprIt = exprIt->next) {
        //arguments.append(operator()(exprIt->expression));
        ++actualArgumentCount;
    }

    Name *functionCallOp = control()->operatorNameId(OperatorNameId::FunctionCallOp);

    foreach (const Result &result, baseResults) {
        FullySpecifiedType ty = result.first.simplified();
        Symbol *lastVisibleSymbol = result.second;

        if (NamedType *namedTy = ty->asNamedType()) {
            const QList<Symbol *> classObjectCandidates = resolveClass(namedTy->name(), result, _context);

            foreach (Symbol *classObject, classObjectCandidates) {
                const QList<Result> overloads = resolveMember(functionCallOp, classObject->asClass(), namedTy->name());

                foreach (const Result &o, overloads) {
                    FullySpecifiedType overloadTy = o.first.simplified();

                    if (Function *funTy = overloadTy->asFunctionType()) {
                        if (maybeValidPrototype(funTy, actualArgumentCount))
                            addResult(funTy->returnType().simplified(), lastVisibleSymbol);
                    }
                }
            }

        } else if (Function *funTy = ty->asFunctionType()) {            
            if (maybeValidPrototype(funTy, actualArgumentCount))
                addResult(funTy->returnType().simplified(), lastVisibleSymbol);

        } else if (Class *classTy = ty->asClassType()) {
            // Constructor call
            FullySpecifiedType ctorTy = control()->namedType(classTy->name());
            addResult(ctorTy, lastVisibleSymbol);
        }
    }

    return false;
}

bool ResolveExpression::visit(ArrayAccessAST *ast)
{
    const QList<Result> baseResults = _results;
    _results.clear();

    const QList<Result> indexResults = operator()(ast->expression);
    ResolveClass resolveClass;

    Name *arrayAccessOp = control()->operatorNameId(OperatorNameId::ArrayAccessOp);

    foreach (const Result &result, baseResults) {
        FullySpecifiedType ty = result.first.simplified();
        Symbol *contextSymbol = result.second;

        if (PointerType *ptrTy = ty->asPointerType()) {
            addResult(ptrTy->elementType().simplified(), contextSymbol);

        } else if (ArrayType *arrTy = ty->asArrayType()) {
            addResult(arrTy->elementType().simplified(), contextSymbol);

        } else if (NamedType *namedTy = ty->asNamedType()) {
            const QList<Symbol *> classObjectCandidates =
                    resolveClass(namedTy->name(), result, _context);

            foreach (Symbol *classObject, classObjectCandidates) {
                Q_ASSERT(classObject->isClass());

                const QList<Result> overloads =
                        resolveMember(arrayAccessOp, classObject->asClass(), namedTy->name());

                foreach (Result r, overloads) {
                    FullySpecifiedType ty = r.first.simplified();
                    if (Function *funTy = ty->asFunctionType()) {
                        ty = funTy->returnType().simplified();
                        addResult(ty, funTy);
                    }
                }
            }
        }
    }
    return false;
}

bool ResolveExpression::visit(MemberAccessAST *ast)
{
    // The candidate types for the base expression are stored in
    // _results.
    QList<Result> baseResults = _results;

    // Evaluate the expression-id that follows the access operator.
    Name *memberName = 0;
    if (ast->member_name)
        memberName = ast->member_name->name;

    // Remember the access operator.
    const int accessOp = tokenKind(ast->access_token);

    _results = resolveMemberExpression(baseResults, accessOp, memberName);

    return false;
}

QList<ResolveExpression::Result>
ResolveExpression::resolveBaseExpression(const QList<Result> &baseResults, int accessOp,
                                         bool *replacedDotOperator) const
{
    QList<Result> results;

    if (baseResults.isEmpty())
        return results;

    Result result = baseResults.first();
    FullySpecifiedType ty = result.first.simplified();
    Symbol *lastVisibleSymbol = result.second;

    if (Function *funTy = ty->asFunctionType()) {
        if (funTy->isAmbiguous())
            ty = funTy->returnType().simplified();
    }

    if (accessOp == T_ARROW)  {
        if (lastVisibleSymbol && ty->isClassType() && ! lastVisibleSymbol->isClass()) {
            // ### remove ! lastVisibleSymbol->isClass() from the condition.
            results.append(Result(ty, lastVisibleSymbol));

        } else if (NamedType *namedTy = ty->asNamedType()) {
            // ### This code is pretty slow.
            const QList<Symbol *> candidates = _context.resolve(namedTy->name());

            foreach (Symbol *candidate, candidates) {
                if (candidate->isTypedef()) {
                    FullySpecifiedType declTy = candidate->type().simplified();
                    const ResolveExpression::Result r(declTy, candidate);

                    // update the result
                    result = r;

                    // refresh the cached ty and lastVisibileSymbol.
                    ty = result.first.simplified();
                    lastVisibleSymbol = result.second;
                    break;
                }
            }
        }

        if (NamedType *namedTy = ty->asNamedType()) {
            ResolveClass resolveClass;
            Name *arrowAccessOp = control()->operatorNameId(OperatorNameId::ArrowOp);
            const QList<Symbol *> candidates = resolveClass(namedTy->name(), result, _context);

            foreach (Symbol *classObject, candidates) {                
                const QList<Result> overloads = resolveMember(arrowAccessOp, classObject->asClass(),
                                                              namedTy->name());

                foreach (const Result &r, overloads) {
                    FullySpecifiedType typeOfOverloadFunction = r.first.simplified();
                    Symbol *lastVisibleSymbol = r.second;
                    Function *funTy = typeOfOverloadFunction->asFunctionType();
                    if (! funTy)
                        continue;

                    typeOfOverloadFunction = funTy->returnType().simplified();

                    if (PointerType *ptrTy = typeOfOverloadFunction->asPointerType()) {
                        FullySpecifiedType elementTy = ptrTy->elementType().simplified();

                        if (elementTy->isNamedType())
                            results.append(Result(elementTy, lastVisibleSymbol));
                    }
                }
            }
        } else if (PointerType *ptrTy = ty->asPointerType()) {
            FullySpecifiedType elementTy = ptrTy->elementType().simplified();

            if (elementTy->isNamedType() || elementTy->isClassType())
                results.append(Result(elementTy, lastVisibleSymbol));
        }
    } else if (accessOp == T_DOT) {
        if (replacedDotOperator) {
            if (PointerType *ptrTy = ty->asPointerType()) {
                *replacedDotOperator = true;
                ty = ptrTy->elementType().simplified();
            } else if (ArrayType *arrTy = ty->asArrayType()) {
                *replacedDotOperator = true;
                ty = arrTy->elementType().simplified();
            }
        }

        if (NamedType *namedTy = ty->asNamedType()) {
            const QList<Scope *> visibleScopes = _context.visibleScopes(result);
            const QList<Symbol *> typedefCandidates = _context.resolve(namedTy->name(), visibleScopes);
            foreach (Symbol *typedefCandidate, typedefCandidates) {
                if (typedefCandidate->isTypedef() && typedefCandidate->type()->isNamedType()) {
                    ty = typedefCandidate->type();
                    lastVisibleSymbol = typedefCandidate;
                    break;
                }
            }

            results.append(Result(ty, lastVisibleSymbol));

        } else if (Function *fun = ty->asFunctionType()) {
            Scope *funScope = fun->scope();

            if (funScope && (funScope->isBlockScope() || funScope->isNamespaceScope())) {
                FullySpecifiedType retTy = fun->returnType().simplified();
                results.append(Result(retTy, lastVisibleSymbol));
            }
        }
    }

    return removeDuplicates(results);
}

QList<ResolveExpression::Result>
ResolveExpression::resolveMemberExpression(const QList<Result> &baseResults,
                                           unsigned accessOp,
                                           Name *memberName,
                                           bool *replacedDotOperator) const
{
    ResolveClass resolveClass;
    QList<Result> results;

    const QList<Result> classObjectResults = resolveBaseExpression(baseResults, accessOp, replacedDotOperator);
    foreach (const Result &r, classObjectResults) {
        FullySpecifiedType ty = r.first;

        if (Class *klass = ty->asClassType())
            results += resolveMember(memberName, klass);

        else if (NamedType *namedTy = ty->asNamedType()) {
            Name *className = namedTy->name();
            const QList<Symbol *> classes = resolveClass(className, r, _context);

            foreach (Symbol *c, classes) {
                if (Class *klass = c->asClass())
                    results += resolveMember(memberName, klass, className);
            }
        }
    }

    return removeDuplicates(results);
}

QList<ResolveExpression::Result>
ResolveExpression::resolveMember(Name *memberName, Class *klass,
                                 Name *className) const
{
    QList<Result> results;

    if (! className)
        className = klass->name();

    if (! className)
        return results;

    QList<Scope *> scopes;
    _context.expand(klass->members(), _context.visibleScopes(), &scopes);

    const QList<Symbol *> candidates = _context.resolve(memberName, scopes);

    foreach (Symbol *candidate, candidates) {
        FullySpecifiedType ty = candidate->type();
        Name *unqualifiedNameId = className;
        
        if (QualifiedNameId *q = className->asQualifiedNameId())
            unqualifiedNameId = q->unqualifiedNameId();
        
        if (TemplateNameId *templId = unqualifiedNameId->asTemplateNameId()) {
            GenTemplateInstance::Substitution subst;
            
            for (unsigned i = 0; i < templId->templateArgumentCount(); ++i) {
                FullySpecifiedType templArgTy = templId->templateArgumentAt(i);
                
                if (i < klass->templateParameterCount()) {
                    Name *templArgName = klass->templateParameterAt(i)->name();
                    if (templArgName && templArgName->identifier()) {
                        Identifier *templArgId = templArgName->identifier();
                        subst.append(qMakePair(templArgId, templArgTy));
                    }
                }
            }
            
            GenTemplateInstance inst(_context, subst);
            ty = inst(candidate);
        }
        
        results.append(Result(ty, candidate));
    }

    return removeDuplicates(results);
}


bool ResolveExpression::visit(PostIncrDecrAST *)
{
    return false;
}

////////////////////////////////////////////////////////////////////////////////
ResolveClass::ResolveClass()
{ }

QList<Symbol *> ResolveClass::operator()(Name *name,
                                         const ResolveExpression::Result &p,
                                         const LookupContext &context)
{
    const QList<ResolveExpression::Result> previousBlackList = _blackList;
    const QList<Symbol *> symbols = resolveClass(name, p, context);
    _blackList = previousBlackList;
    return symbols;
}

QList<Symbol *> ResolveClass::resolveClass(Name *name,
                                           const ResolveExpression::Result &p,
                                           const LookupContext &context)
{
    QList<Symbol *> resolvedSymbols;

    if (_blackList.contains(p))
        return resolvedSymbols;

    _blackList.append(p);

    const QList<Symbol *> candidates =
            context.resolve(name, context.visibleScopes(p));

    foreach (Symbol *candidate, candidates) {
        if (Class *klass = candidate->asClass()) {
            if (resolvedSymbols.contains(klass))
                continue; // we already know about `klass'
            resolvedSymbols.append(klass);
        } else if (candidate->isTypedef()) {
            if (Declaration *decl = candidate->asDeclaration()) {
                if (Class *asClass = decl->type()->asClassType()) {
                    // typedef struct { } Point;
                    // Point pt;
                    // pt.
                    resolvedSymbols.append(asClass);
                } else {
                    // typedef Point Boh;
                    // Boh b;
                    // b.
                    FullySpecifiedType declType = decl->type().simplified();
                    if (NamedType *namedTy = declType->asNamedType()) {
                        const ResolveExpression::Result r(declType, decl);
                        resolvedSymbols += resolveClass(namedTy->name(), r, context);
                    }
                }
            }
        } else if (Declaration *decl = candidate->asDeclaration()) {
            if (Function *funTy = decl->type()->asFunctionType()) {
                // QString foo("ciao");
                // foo.
                if (funTy->scope() && (funTy->scope()->isBlockScope() || funTy->scope()->isNamespaceScope())) {
                    FullySpecifiedType retTy = funTy->returnType().simplified();
                    if (NamedType *namedTy = retTy->asNamedType()) {
                        const ResolveExpression::Result r(retTy, decl);
                        resolvedSymbols += resolveClass(namedTy->name(), r, context);
                    }
                }
            }
        }
    }

    return resolvedSymbols;
}
