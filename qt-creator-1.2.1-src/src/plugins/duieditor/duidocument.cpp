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

#include "duidocument.h"
#include "javascriptast_p.h"
#include "javascriptlexer_p.h"
#include "javascriptparser_p.h"
#include "javascriptengine_p.h"
#include "javascriptnodepool_p.h"

using namespace DuiEditor;
using namespace DuiEditor::Internal;
using namespace JavaScript;

DuiDocument::DuiDocument(const QString &fileName)
	: _engine(0), _pool(0), _program(0), _fileName(fileName)
{
}

DuiDocument::~DuiDocument()
{
	delete _engine;
	delete _pool;
}

DuiDocument::Ptr DuiDocument::create(const QString &fileName)
{
	DuiDocument::Ptr doc(new DuiDocument(fileName));
	return doc;
}

AST::UiProgram *DuiDocument::program() const
{
	return _program;
}

QList<DiagnosticMessage> DuiDocument::diagnosticMessages() const
{
	return _diagnosticMessages;
}

void DuiDocument::setSource(const QString &source)
{
	_source = source;
}

bool DuiDocument::parse()
{
	Q_ASSERT(! _engine);
	Q_ASSERT(! _pool);
	Q_ASSERT(! _program);

	_engine = new Engine();
	_pool = new NodePool(_fileName, _engine);

	Lexer lexer(_engine);
	Parser parser(_engine);

	lexer.setCode(_source, /*line = */ 1);

	bool parsed = parser.parse();
	_program = parser.ast();
	_diagnosticMessages = parser.diagnosticMessages();
	return parsed;
}

Snapshot::Snapshot()
{
}

Snapshot::~Snapshot()
{
}


