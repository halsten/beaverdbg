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

#include "cpptools_global.h"

namespace CppTools {

enum DoxygenReservedWord {
    T_DOXY_IDENTIFIER,
    T_DOXY_ARG,
    T_DOXY_ATTENTION,
    T_DOXY_AUTHOR,
    T_DOXY_CALLGRAPH,
    T_DOXY_CODE,
    T_DOXY_DOT,
    T_DOXY_ELSE,
    T_DOXY_ENDCODE,
    T_DOXY_ENDCOND,
    T_DOXY_ENDDOT,
    T_DOXY_ENDHTMLONLY,
    T_DOXY_ENDIF,
    T_DOXY_ENDLATEXONLY,
    T_DOXY_ENDLINK,
    T_DOXY_ENDMANONLY,
    T_DOXY_ENDVERBATIM,
    T_DOXY_ENDXMLONLY,
    T_DOXY_HIDEINITIALIZER,
    T_DOXY_HTMLONLY,
    T_DOXY_INTERFACE,
    T_DOXY_INTERNAL,
    T_DOXY_INVARIANT,
    T_DOXY_LATEXONLY,
    T_DOXY_LI,
    T_DOXY_MANONLY,
    T_DOXY_N,
    T_DOXY_NOSUBGROUPING,
    T_DOXY_NOTE,
    T_DOXY_ONLY,
    T_DOXY_POST,
    T_DOXY_PRE,
    T_DOXY_REMARKS,
    T_DOXY_RETURN,
    T_DOXY_RETURNS,
    T_DOXY_SA,
    T_DOXY_SEE,
    T_DOXY_SHOWINITIALIZER,
    T_DOXY_SINCE,
    T_DOXY_TEST,
    T_DOXY_TODO,
    T_DOXY_VERBATIM,
    T_DOXY_WARNING,
    T_DOXY_XMLONLY,
    T_DOXY_A,
    T_DOXY_ADDTOGROUP,
    T_DOXY_ANCHOR,
    T_DOXY_B,
    T_DOXY_C,
    T_DOXY_CLASS,
    T_DOXY_COND,
    T_DOXY_COPYDOC,
    T_DOXY_DEF,
    T_DOXY_DONTINCLUDE,
    T_DOXY_DOTFILE,
    T_DOXY_E,
    T_DOXY_ELSEIF,
    T_DOXY_EM,
    T_DOXY_ENUM,
    T_DOXY_EXAMPLE,
    T_DOXY_EXCEPTION,
    T_DOXY_EXCEPTIONS,
    T_DOXY_FILE,
    T_DOXY_HTMLINCLUDE,
    T_DOXY_IF,
    T_DOXY_IFNOT,
    T_DOXY_INCLUDE,
    T_DOXY_LINK,
    T_DOXY_NAMESPACE,
    T_DOXY_P,
    T_DOXY_PACKAGE,
    T_DOXY_REF,
    T_DOXY_RELATES,
    T_DOXY_RELATESALSO,
    T_DOXY_RETVAL,
    T_DOXY_THROW,
    T_DOXY_THROWS,
    T_DOXY_VERBINCLUDE,
    T_DOXY_VERSION,
    T_DOXY_XREFITEM,
    T_DOXY_PARAM,
    T_DOXY_IMAGE,
    T_DOXY_DEFGROUP,
    T_DOXY_PAGE,
    T_DOXY_PARAGRAPH,
    T_DOXY_SECTION,
    T_DOXY_STRUCT,
    T_DOXY_SUBSECTION,
    T_DOXY_SUBSUBSECTION,
    T_DOXY_UNION,
    T_DOXY_WEAKGROUP,
    T_DOXY_ADDINDEX,
    T_DOXY_BRIEF,
    T_DOXY_BUG,
    T_DOXY_DATE,
    T_DOXY_DEPRECATED,
    T_DOXY_FN,
    T_DOXY_INGROUP,
    T_DOXY_LINE,
    T_DOXY_MAINPAGE,
    T_DOXY_NAME,
    T_DOXY_OVERLOAD,
    T_DOXY_PAR,
    T_DOXY_SHORT,
    T_DOXY_SKIP,
    T_DOXY_SKIPLINE,
    T_DOXY_TYPEDEF,
    T_DOXY_UNTIL,
    T_DOXY_VAR,

    T_FIRST_QDOC_TAG,

    T_DOXY_ABSTRACT = T_FIRST_QDOC_TAG,
    T_DOXY_BADCODE,
    T_DOXY_BASENAME,
    T_DOXY_BOLD,
    T_DOXY_CAPTION,
    T_DOXY_CHAPTER,
    T_DOXY_CODELINE,
    T_DOXY_DOTS,
    T_DOXY_ENDABSTRACT,
    T_DOXY_ENDCHAPTER,
    T_DOXY_ENDFOOTNOTE,
    T_DOXY_ENDLEGALESE,
    T_DOXY_ENDLIST,
    T_DOXY_ENDOMIT,
    T_DOXY_ENDPART,
    T_DOXY_ENDQUOTATION,
    T_DOXY_ENDRAW,
    T_DOXY_ENDSECTION1,
    T_DOXY_ENDSECTION2,
    T_DOXY_ENDSECTION3,
    T_DOXY_ENDSECTION4,
    T_DOXY_ENDSIDEBAR,
    T_DOXY_ENDTABLE,
    T_DOXY_EXPIRE,
    T_DOXY_FOOTNOTE,
    T_DOXY_GENERATELIST,
    T_DOXY_GRANULARITY,
    T_DOXY_HEADER,
    T_DOXY_I,
    T_DOXY_INDEX,
    T_DOXY_INLINEIMAGE,
    T_DOXY_KEYWORD,
    T_DOXY_L,
    T_DOXY_LEGALESE,
    T_DOXY_LIST,
    T_DOXY_META,
    T_DOXY_NEWCODE,
    T_DOXY_O,
    T_DOXY_OLDCODE,
    T_DOXY_OMIT,
    T_DOXY_OMITVALUE,
    T_DOXY_PART,
    T_DOXY_PRINTLINE,
    T_DOXY_PRINTTO,
    T_DOXY_PRINTUNTIL,
    T_DOXY_QUOTATION,
    T_DOXY_QUOTEFILE,
    T_DOXY_QUOTEFROMFILE,
    T_DOXY_QUOTEFUNCTION,
    T_DOXY_RAW,
    T_DOXY_ROW,
    T_DOXY_SECTION1,
    T_DOXY_SECTION2,
    T_DOXY_SECTION3,
    T_DOXY_SECTION4,
    T_DOXY_SIDEBAR,
    T_DOXY_SKIPTO,
    T_DOXY_SKIPUNTIL,
    T_DOXY_SNIPPET,
    T_DOXY_SUB,
    T_DOXY_SUP,
    T_DOXY_TABLE,
    T_DOXY_TABLEOFCONTENTS,
    T_DOXY_TARGET,
    T_DOXY_TT,
    T_DOXY_UNDERLINE,
    T_DOXY_UNICODE,
    T_DOXY_VALUE,
    T_DOXY_CONTENTSPAGE,
    T_DOXY_EXTERNALPAGE,
    T_DOXY_GROUP,
    T_DOXY_HEADERFILE,
    T_DOXY_INDEXPAGE,
    T_DOXY_INHEADERFILE,
    T_DOXY_MACRO,
    T_DOXY_MODULE,
    T_DOXY_NEXTPAGE,
    T_DOXY_PREVIOUSPAGE,
    T_DOXY_PROPERTY,
    T_DOXY_REIMP,
    T_DOXY_SERVICE,
    T_DOXY_STARTPAGE,
    T_DOXY_VARIABLE,
    T_DOXY_COMPAT,
    T_DOXY_INMODULE,
    T_DOXY_MAINCLASS,
    T_DOXY_NONREENTRANT,
    T_DOXY_OBSOLETE,
    T_DOXY_PRELIMINARY,
    T_DOXY_INPUBLICGROUP,
    T_DOXY_REENTRANT,
    T_DOXY_SUBTITLE,
    T_DOXY_THREADSAFE,
    T_DOXY_TITLE,
    T_DOXY_CORELIB,
    T_DOXY_UITOOLS,
    T_DOXY_GUI,
    T_DOXY_NETWORK,
    T_DOXY_OPENGL,
    T_DOXY_QT3SUPPORT,
    T_DOXY_SVG,
    T_DOXY_SQL,
    T_DOXY_QTESTLIB,
    T_DOXY_WEBKIT,
    T_DOXY_XML,

    T_DOXY_LAST_TAG
};

CPPTOOLS_EXPORT int classifyDoxygenTag(const QChar *s, int n);
CPPTOOLS_EXPORT const char *doxygenTagSpell(int index);

} // namespace ::CppTools

