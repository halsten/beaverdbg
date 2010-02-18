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

#include <AST.h>
#include <ASTVisitor.h>
#include <Control.h>
#include <Scope.h>
#include <Semantic.h>
#include <TranslationUnit.h>
#include <Literals.h>
#include <Symbols.h>
#include <Names.h>
#include <CoreTypes.h>

#include <string>
#include <cstdlib>
#include <cstdlib>

using namespace CPlusPlus;

int main(int argc, char *argv[])
{
    std::string cmdline;
    cmdline += "gcc -E -xc++ -U__BLOCKS__";

    for (int i = 1; i < argc; ++i) {
        cmdline += ' ';
        cmdline += argv[i];
    }

    enum { BLOCK_SIZE = 4 * 1024};
    char block[BLOCK_SIZE];

    std::string source;

    if (FILE *fp = popen(cmdline.c_str(), "r")) {
        while (size_t sz = fread(block, 1, BLOCK_SIZE, fp))
            source.append(block, sz);

        pclose(fp);

    } else {
        fprintf(stderr, "c++: No such file or directory\n");
        return EXIT_FAILURE;
    }

    Control control;
    TranslationUnit unit(&control, control.findOrInsertStringLiteral("<stdin>"));
    unit.setSource(source.c_str(), source.size());
    unit.parse();

    return EXIT_SUCCESS;
}
