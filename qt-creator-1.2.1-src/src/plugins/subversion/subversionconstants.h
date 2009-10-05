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

#ifndef SUBVERSION_CONSTANTS_H
#define SUBVERSION_CONSTANTS_H

namespace Subversion {
namespace Constants {

const char * const SUBVERSION_SUBMIT_MIMETYPE = "application/vnd.nokia.text.subversion.submit";
const char * const SUBVERSIONEDITOR  = "Subversion Editor";
const char * const SUBVERSIONEDITOR_KIND  = "Subversion Editor";
const char * const SUBVERSIONCOMMITEDITOR  = "Subversion Commit Editor";
const char * const SUBVERSIONCOMMITEDITOR_KIND  = "Subversion Commit Editor";
const char * const SUBMIT_CURRENT = "Subversion.SubmitCurrentLog";
const char * const DIFF_SELECTED = "Subversion.DiffSelectedFilesInLog";
enum { debug = 0 };

} // namespace Constants
} // namespace SubVersion

#endif // SUBVERSION_CONSTANTS_H
