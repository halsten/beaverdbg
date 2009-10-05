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

#include "cdbsymbolgroupcontext.h"
#include "cdbdebugengine_p.h"
#include "watchhandler.h"
#include "watchutils.h"

#include <QtCore/QTextStream>

enum { debug = 0 };

static inline QString msgSymbolNotFound(const QString &s)
{
    return QString::fromLatin1("The symbol '%1' could not be found.").arg(s);
}

static inline bool isTopLevelSymbol(const DEBUG_SYMBOL_PARAMETERS &p)
{
    return p.ParentSymbol == DEBUG_ANY_ID;
}

static inline void debugSymbolFlags(unsigned long f, QTextStream &str)
{
    if (f & DEBUG_SYMBOL_EXPANDED)
        str << "DEBUG_SYMBOL_EXPANDED";
    if (f & DEBUG_SYMBOL_READ_ONLY)
        str << "|DEBUG_SYMBOL_READ_ONLY";
    if (f & DEBUG_SYMBOL_IS_ARRAY)
        str << "|DEBUG_SYMBOL_IS_ARRAY";
    if (f & DEBUG_SYMBOL_IS_FLOAT)
        str << "|DEBUG_SYMBOL_IS_FLOAT";
    if (f & DEBUG_SYMBOL_IS_ARGUMENT)
        str << "|DEBUG_SYMBOL_IS_ARGUMENT";
    if (f & DEBUG_SYMBOL_IS_LOCAL)
        str << "|DEBUG_SYMBOL_IS_LOCAL";
}

QTextStream &operator<<(QTextStream &str, const DEBUG_SYMBOL_PARAMETERS &p)
{
    str << " Type=" << p.TypeId << " parent=";
    if (isTopLevelSymbol(p)) {
        str << "<ROOT>";
    } else {
        str << p.ParentSymbol;
    }
    str << " Subs=" << p.SubElements << " flags=" << p.Flags << '/';
    debugSymbolFlags(p.Flags, str);
    return str;
}

// A helper function to extract a string value from a member function of
// IDebugSymbolGroup2 taking the symbol index and a character buffer.
// Pass in the the member function as '&IDebugSymbolGroup2::GetSymbolNameWide'

typedef HRESULT  (__stdcall IDebugSymbolGroup2::*WideStringRetrievalFunction)(ULONG, PWSTR, ULONG, PULONG);

static inline QString getSymbolString(IDebugSymbolGroup2 *sg,
                                      WideStringRetrievalFunction wsf,
                                      unsigned long index)
{
    static WCHAR nameBuffer[MAX_PATH + 1];
    // Name
    ULONG nameLength;
    const HRESULT hr = (sg->*wsf)(index, nameBuffer, MAX_PATH, &nameLength);
    if (SUCCEEDED(hr)) {
        nameBuffer[nameLength] = 0;
        return QString::fromUtf16(reinterpret_cast<const ushort *>(nameBuffer));
    }
    return QString();
}

namespace Debugger {
namespace Internal {

static inline CdbSymbolGroupContext::SymbolState getSymbolState(const DEBUG_SYMBOL_PARAMETERS &p)
{
    if (p.SubElements == 0u)
        return CdbSymbolGroupContext::LeafSymbol;
    return (p.Flags & DEBUG_SYMBOL_EXPANDED) ?
               CdbSymbolGroupContext::ExpandedSymbol :
               CdbSymbolGroupContext::CollapsedSymbol;
}

CdbSymbolGroupContext::CdbSymbolGroupContext(const QString &prefix,
                                             CIDebugSymbolGroup *symbolGroup) :
    m_prefix(prefix),
    m_nameDelimiter(QLatin1Char('.')),
    m_symbolGroup(symbolGroup),
    m_unnamedSymbolNumber(1)
{
}

CdbSymbolGroupContext::~CdbSymbolGroupContext()
{
    m_symbolGroup->Release();
}

CdbSymbolGroupContext *CdbSymbolGroupContext::create(const QString &prefix,
                                                     CIDebugSymbolGroup *symbolGroup,
                                                     QString *errorMessage)
{
    CdbSymbolGroupContext *rc = new CdbSymbolGroupContext(prefix, symbolGroup);
    if (!rc->init(errorMessage)) {
        delete rc;
        return 0;
    }
    return rc;
}

bool CdbSymbolGroupContext::init(QString *errorMessage)
{
    // retrieve the root symbols
    ULONG count;
    HRESULT hr = m_symbolGroup->GetNumberSymbols(&count);
    if (FAILED(hr)) {
        *errorMessage = msgComFailed("GetNumberSymbols", hr);
        return false;
    }

    if (count) {
        m_symbolParameters.reserve(3u * count);
        m_symbolParameters.resize(count);

        hr = m_symbolGroup->GetSymbolParameters(0, count, symbolParameters());
        if (FAILED(hr)) {
            *errorMessage = QString::fromLatin1("In %1: %2 (%3 symbols)").arg(QLatin1String(Q_FUNC_INFO), msgComFailed("GetSymbolParameters", hr)).arg(count);
            return false;
        }
        populateINameIndexMap(m_prefix, DEBUG_ANY_ID, 0, count);
    }
    if (debug)
        qDebug() << Q_FUNC_INFO << '\n'<< toString();
    return true;
}

void CdbSymbolGroupContext::populateINameIndexMap(const QString &prefix, unsigned long parentId,
                                                  unsigned long start, unsigned long count)
{
    // Make the entries for iname->index mapping. We might encounter
    // already expanded subitems when doing it for top-level, recurse in that case.
    const QString symbolPrefix = prefix + m_nameDelimiter;
    if (debug)
        qDebug() << Q_FUNC_INFO << '\n'<< symbolPrefix << start << count;
    const unsigned long end = m_symbolParameters.size();
    unsigned long seenChildren = 0;
    // Skip over expanded children
    for (unsigned long i = start; i < end && seenChildren < count; i++) {
        const DEBUG_SYMBOL_PARAMETERS &p = m_symbolParameters.at(i);
        if (parentId == p.ParentSymbol) {
            seenChildren++;
            // "__formal" occurs when someone writes "void foo(int /* x */)..."
            static const QString unnamedFormalParameter = QLatin1String("__formal");
            QString symbolName = getSymbolString(m_symbolGroup, &IDebugSymbolGroup2::GetSymbolNameWide, i);
            if (symbolName == unnamedFormalParameter) {
                symbolName = QLatin1String("<unnamed");
                symbolName += QString::number(m_unnamedSymbolNumber++);
                symbolName += QLatin1Char('>');
            }
            const QString name = symbolPrefix + symbolName;
            m_inameIndexMap.insert(name, i);
            if (getSymbolState(p) == ExpandedSymbol)
                populateINameIndexMap(name, i, i + 1, p.SubElements);
        }
    }
}

QString CdbSymbolGroupContext::toString(bool verbose) const
{
    QString rc;
    QTextStream str(&rc);
    const int count = m_symbolParameters.size();
    for (int i = 0; i < count; i++) {
        str << i << ' ';
        const DEBUG_SYMBOL_PARAMETERS &p = m_symbolParameters.at(i);
        if (!isTopLevelSymbol(p))
            str << "    ";
        str << getSymbolString(m_symbolGroup, &IDebugSymbolGroup2::GetSymbolNameWide, i);
        if (p.Flags & DEBUG_SYMBOL_IS_LOCAL)
            str << " '" << getSymbolString(m_symbolGroup, &IDebugSymbolGroup2::GetSymbolTypeNameWide, i);
        str << p << '\n';
    }
    if (verbose) {
        str << "NameIndexMap\n";
        NameIndexMap::const_iterator ncend = m_inameIndexMap.constEnd();
        for (NameIndexMap::const_iterator it = m_inameIndexMap.constBegin() ; it != ncend; ++it)
            str << it.key() << ' ' << it.value() << '\n';
    }
    return rc;
}

CdbSymbolGroupContext::SymbolState CdbSymbolGroupContext::symbolState(unsigned long index) const
{
    return getSymbolState(m_symbolParameters.at(index));
}

CdbSymbolGroupContext::SymbolState CdbSymbolGroupContext::symbolState(const QString &prefix) const
{
    if (prefix == m_prefix) // root
        return ExpandedSymbol;
    unsigned long index;
    if (!lookupPrefix(prefix, &index)) {
        qWarning("WARNING %s: %s\n", Q_FUNC_INFO, qPrintable(msgSymbolNotFound(prefix)));
        return LeafSymbol;
    }
    return symbolState(index);
}

// Find index of a prefix
bool CdbSymbolGroupContext::lookupPrefix(const QString &prefix, unsigned long *index) const
{
    *index = 0;
    const NameIndexMap::const_iterator it = m_inameIndexMap.constFind(prefix);
    if (it == m_inameIndexMap.constEnd())
        return false;
    *index = it.value();
    return true;
}

/* Retrieve children and get the position. */
bool CdbSymbolGroupContext::getChildSymbolsPosition(const QString &prefix,
                                                    unsigned long *start,
                                                    unsigned long *parentId,
                                                    QString *errorMessage)
{
    if (debug)
        qDebug() << Q_FUNC_INFO << '\n'<< prefix;

    *start = *parentId = 0;
    // Root item?
    if (prefix == m_prefix) {
        *start = 0;
        *parentId = DEBUG_ANY_ID;
        if (debug)
            qDebug() << '<' << prefix << "at" << *start;
        return true;
    }
    // Get parent index, make sure it is expanded
    NameIndexMap::const_iterator nit = m_inameIndexMap.constFind(prefix);
    if (nit == m_inameIndexMap.constEnd()) {
        *errorMessage = QString::fromLatin1("'%1' not found.").arg(prefix);
        return false;
    }
    *parentId = nit.value();
    *start = nit.value() + 1;
    if (!expandSymbol(prefix, *parentId, errorMessage))
        return false;
    if (debug)
        qDebug() << '<' << prefix << "at" << *start;
    return true;
}

static inline QString msgExpandFailed(const QString &prefix, unsigned long index, const QString &why)
{
    return QString::fromLatin1("Unable to expand '%1' %2: %3").arg(prefix).arg(index).arg(why);
}

// Expand a symbol using the symbol group interface.
bool CdbSymbolGroupContext::expandSymbol(const QString &prefix, unsigned long index, QString *errorMessage)
{
    if (debug)
        qDebug() << '>' << Q_FUNC_INFO << '\n' << prefix << index;

    switch (symbolState(index)) {
    case LeafSymbol:
        *errorMessage = QString::fromLatin1("Attempt to expand leaf node '%1' %2!").arg(prefix).arg(index);
        return false;
    case ExpandedSymbol:
        return true;
    case CollapsedSymbol:
        break;
    }

    HRESULT hr = m_symbolGroup->ExpandSymbol(index, TRUE);
    if (FAILED(hr)) {
        *errorMessage = msgExpandFailed(prefix, index, msgComFailed("ExpandSymbol", hr));
        return false;
    }
    // Hopefully, this will never fail, else data structure will be foobar.
    const ULONG oldSize = m_symbolParameters.size();
    ULONG newSize;
    hr = m_symbolGroup->GetNumberSymbols(&newSize);
    if (FAILED(hr)) {
        *errorMessage = msgExpandFailed(prefix, index, msgComFailed("GetNumberSymbols", hr));
        return false;
    }

    // Retrieve the new parameter structs which will be inserted
    // after the parents, offsetting consecutive indexes.
    m_symbolParameters.resize(newSize);

    hr = m_symbolGroup->GetSymbolParameters(0, newSize, symbolParameters());
    if (FAILED(hr)) {
        *errorMessage = msgExpandFailed(prefix, index, msgComFailed("GetSymbolParameters", hr));
        return false;
    }
    // The new symbols are inserted after the parent symbol.
    // We need to correct the following values in the name->index map
    const unsigned long newSymbolCount = newSize - oldSize;
    const NameIndexMap::iterator nend = m_inameIndexMap.end();
    for (NameIndexMap::iterator it = m_inameIndexMap.begin(); it != nend; ++it)
        if (it.value() > index)
            it.value() += newSymbolCount;
    // insert the new symbols
    populateINameIndexMap(prefix, index, index + 1, newSymbolCount);
    if (debug > 1)
        qDebug() << '<' << Q_FUNC_INFO << '\n' << prefix << index << '\n' << toString();
    return true;
}

void CdbSymbolGroupContext::clear()
{
    m_symbolParameters.clear();
    m_inameIndexMap.clear();
}

int CdbSymbolGroupContext::getDisplayableChildCount(unsigned long index) const
{
    if (!isExpanded(index))
        return 0;
    int rc = 0;
    // Skip over expanded children, count displayable ones
    const unsigned long childCount = m_symbolParameters.at(index).SubElements;
    unsigned long seenChildren = 0;
    for (unsigned long c = index + 1; seenChildren < childCount; c++) {
        const DEBUG_SYMBOL_PARAMETERS &params = m_symbolParameters.at(c);
        if (params.ParentSymbol == index) {
            seenChildren++;
            if (isSymbolDisplayable(params))
                rc++;
        }
    }
    return rc;
}

QString CdbSymbolGroupContext::symbolINameAt(unsigned long index) const
{
    return m_inameIndexMap.key(index);
}

static inline QString hexSymbolOffset(CIDebugSymbolGroup *sg, unsigned long index)
{
    ULONG64 rc = 0;
    if (FAILED(sg->GetSymbolOffset(index, &rc)))
        rc = 0;
    return QLatin1String("0x") + QString::number(rc, 16);
}

WatchData CdbSymbolGroupContext::symbolAt(unsigned long index) const
{
    WatchData wd;
    wd.iname = symbolINameAt(index);
    wd.exp = wd.iname;
    const int lastDelimiterPos = wd.iname.lastIndexOf(m_nameDelimiter);
    wd.name = lastDelimiterPos == -1 ? wd.iname : wd.iname.mid(lastDelimiterPos + 1);
    wd.addr = hexSymbolOffset(m_symbolGroup, index);
    wd.setType(getSymbolString(m_symbolGroup, &IDebugSymbolGroup2::GetSymbolTypeNameWide, index));
    wd.setValue(getSymbolString(m_symbolGroup, &IDebugSymbolGroup2::GetSymbolValueTextWide, index).toUtf8());
    wd.setChildrenNeeded(); // compensate side effects of above setters
    wd.setChildCountNeeded();
    // Figure out children. The SubElement is only a guess unless the symbol,
    // is expanded, so, we leave this as a guess for later updates.
    // If the symbol has children (expanded or not), we leave the 'Children' flag
    // in 'needed' state.
    const DEBUG_SYMBOL_PARAMETERS &params = m_symbolParameters.at(index);
    if (params.SubElements) {
        if (getSymbolState(params) == ExpandedSymbol)
            wd.setChildCount(getDisplayableChildCount(index));
    } else {
        wd.setChildCount(0);
    }
    if (debug > 1)
        qDebug() << Q_FUNC_INFO << index << '\n' << wd.toString();
    return wd;
}

bool CdbSymbolGroupContext::assignValue(const QString &iname, const QString &value,
                                        QString *newValue, QString *errorMessage)
{
    if (debugCDB)
        qDebug() << Q_FUNC_INFO << '\n' << iname << value;
    const NameIndexMap::const_iterator it = m_inameIndexMap.constFind(iname);
    if (it == m_inameIndexMap.constEnd()) {
        *errorMessage = msgSymbolNotFound(iname);
        return false;
    }
    const unsigned long  index = it.value();
    const HRESULT hr = m_symbolGroup->WriteSymbolWide(index, reinterpret_cast<PCWSTR>(value.utf16()));
    if (FAILED(hr)) {
        *errorMessage = QString::fromLatin1("Unable to assign '%1' to '%2': %3").
                        arg(value, iname, msgComFailed("WriteSymbolWide", hr));
        return false;
    }
    if (newValue)
        *newValue = getSymbolString(m_symbolGroup, &IDebugSymbolGroup2::GetSymbolValueTextWide, index);
    return true;
}

// format an array of integers as "0x323, 0x2322, ..."
template <class Integer>
static QString formatArrayHelper(const Integer *array, int size, int base = 10)
{
    QString rc;
    const QString hexPrefix = QLatin1String("0x");
    const QString separator = QLatin1String(", ");
    const bool hex = base == 16;
    for (int i = 0; i < size; i++) {
        if (i)
            rc += separator;
        if (hex)
            rc += hexPrefix;
        rc += QString::number(array[i], base);
    }
    return rc;
}

QString CdbSymbolGroupContext::hexFormatArray(const unsigned short *array, int size)
{
    return formatArrayHelper(array, size, 16);
}

// Helper to format an integer with
// a hex prefix in case base = 16
template <class Integer>
        inline QString formatInteger(Integer value, int base)
{
    QString rc;
    if (base == 16)
        rc = QLatin1String("0x");
    rc += QString::number(value, base);
    return rc;
}

QString CdbSymbolGroupContext::debugValueToString(const DEBUG_VALUE &dv, CIDebugControl *ctl,
                                                  QString *qType,
                                                  int integerBase)
{
    switch (dv.Type) {
    case DEBUG_VALUE_INT8:
        if (qType)
            *qType = QLatin1String("char");
        return formatInteger(dv.I8, integerBase);
    case DEBUG_VALUE_INT16:
        if (qType)
            *qType = QLatin1String("short");
        return formatInteger(static_cast<short>(dv.I16), integerBase);
    case DEBUG_VALUE_INT32:
        if (qType)
            *qType = QLatin1String("long");
        return formatInteger(static_cast<long>(dv.I32), integerBase);
    case DEBUG_VALUE_INT64:
        if (qType)
            *qType = QLatin1String("long long");
        return formatInteger(static_cast<long long>(dv.I64), integerBase);
    case DEBUG_VALUE_FLOAT32:
        if (qType)
            *qType = QLatin1String("float");
        return QString::number(dv.F32);
    case DEBUG_VALUE_FLOAT64:
        if (qType)
            *qType = QLatin1String("double");
        return QString::number(dv.F64);
    case DEBUG_VALUE_FLOAT80:
    case DEBUG_VALUE_FLOAT128: { // Convert to double
            DEBUG_VALUE doubleValue;
            double d = 0.0;
            if (SUCCEEDED(ctl->CoerceValue(const_cast<DEBUG_VALUE*>(&dv), DEBUG_VALUE_FLOAT64, &doubleValue)))
                d = dv.F64;
            if (qType)
                *qType = QLatin1String(dv.Type == DEBUG_VALUE_FLOAT80 ? "80bit-float" : "128bit-float");
            return QString::number(d);
        }
    case DEBUG_VALUE_VECTOR64: {
            if (qType)
                *qType = QLatin1String("64bit-vector");
            QString rc = QLatin1String("bytes: ");
            rc += formatArrayHelper(dv.VI8, 8, integerBase);
            rc += QLatin1String(" long: ");
            rc += formatArrayHelper(dv.VI32, 2, integerBase);
            return rc;
        }
    case DEBUG_VALUE_VECTOR128: {
            if (qType)
                *qType = QLatin1String("128bit-vector");
            QString rc = QLatin1String("bytes: ");
            rc += formatArrayHelper(dv.VI8, 16, integerBase);
            rc += QLatin1String(" long long: ");
            rc += formatArrayHelper(dv.VI64, 2, integerBase);
            return rc;
        }
    }
    if (qType)
        *qType = QString::fromLatin1("Unknown type #%1:").arg(dv.Type);
    return formatArrayHelper(dv.RawBytes, 24, integerBase);
}

bool CdbSymbolGroupContext::debugValueToInteger(const DEBUG_VALUE &dv, qint64 *value)
{
    *value = 0;
    switch (dv.Type) {
    case DEBUG_VALUE_INT8:
        *value = dv.I8;
        return true;
    case DEBUG_VALUE_INT16:
        *value = static_cast<short>(dv.I16);
        return true;
    case DEBUG_VALUE_INT32:
        *value = static_cast<long>(dv.I32);
        return true;
    case DEBUG_VALUE_INT64:
        *value = static_cast<long long>(dv.I64);
        return true;
    default:
        break;
    }
    return false;
}

} // namespace Internal
} // namespace Debugger
