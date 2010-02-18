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

#include "texteditorsettings.h"

#include "basetexteditor.h"
#include "behaviorsettingspage.h"
#include "displaysettings.h"
#include "displaysettingspage.h"
#include "fontsettingspage.h"
#include "storagesettings.h"
#include "tabsettings.h"
#include "texteditorconstants.h"
#include "texteditorplugin.h"

#include <extensionsystem/pluginmanager.h>
#include <utils/qtcassert.h>

#include <QtGui/QApplication>

using namespace TextEditor;
using namespace TextEditor::Constants;

TextEditorSettings *TextEditorSettings::m_instance = 0;

TextEditorSettings::TextEditorSettings(QObject *parent)
    : QObject(parent)
{
    QTC_ASSERT(!m_instance, return);
    m_instance = this;

    ExtensionSystem::PluginManager *pm = ExtensionSystem::PluginManager::instance();

    // Note: default background colors are coming from FormatDescription::background()

    // Add font preference page
    FormatDescriptions formatDescriptions;
    formatDescriptions.append(FormatDescription(QLatin1String(C_TEXT), tr("Text")));

    // Special categories
    const QPalette p = QApplication::palette();
    formatDescriptions.append(FormatDescription(QLatin1String(C_LINK), tr("Link"), Qt::blue));
    formatDescriptions.append(FormatDescription(QLatin1String(C_SELECTION), tr("Selection"), p.color(QPalette::HighlightedText)));
    formatDescriptions.append(FormatDescription(QLatin1String(C_LINE_NUMBER), tr("Line Number")));
    formatDescriptions.append(FormatDescription(QLatin1String(C_SEARCH_RESULT), tr("Search Result")));
    formatDescriptions.append(FormatDescription(QLatin1String(C_SEARCH_SCOPE), tr("Search Scope")));
    formatDescriptions.append(FormatDescription(QLatin1String(C_PARENTHESES), tr("Parentheses")));
    formatDescriptions.append(FormatDescription(QLatin1String(C_CURRENT_LINE), tr("Current Line")));

    FormatDescription currentLineNumber = FormatDescription(QLatin1String(C_CURRENT_LINE_NUMBER), tr("Current Line Number"), Qt::darkGray);
    currentLineNumber.format().setBold(true);
    formatDescriptions.append(currentLineNumber);

    formatDescriptions.append(FormatDescription(QLatin1String(C_OCCURRENCES), tr("Occurrences")));
    formatDescriptions.append(FormatDescription(QLatin1String(C_OCCURRENCES_UNUSED), tr("Unused Occurrence")));
    formatDescriptions.append(FormatDescription(QLatin1String(C_OCCURRENCES_RENAME), tr("Renaming Occurrence")));

    // Standard categories
    formatDescriptions.append(FormatDescription(QLatin1String(C_NUMBER), tr("Number"), Qt::darkBlue));
    formatDescriptions.append(FormatDescription(QLatin1String(C_STRING), tr("String"), Qt::darkGreen));
    formatDescriptions.append(FormatDescription(QLatin1String(C_TYPE), tr("Type"), Qt::darkMagenta));
    formatDescriptions.append(FormatDescription(QLatin1String(C_KEYWORD), tr("Keyword"), Qt::darkYellow));
    formatDescriptions.append(FormatDescription(QLatin1String(C_OPERATOR), tr("Operator")));
    formatDescriptions.append(FormatDescription(QLatin1String(C_PREPROCESSOR), tr("Preprocessor"), Qt::darkBlue));
    formatDescriptions.append(FormatDescription(QLatin1String(C_LABEL), tr("Label"), Qt::darkRed));
    formatDescriptions.append(FormatDescription(QLatin1String(C_COMMENT), tr("Comment"), Qt::darkGreen));
    formatDescriptions.append(FormatDescription(QLatin1String(C_DOXYGEN_COMMENT), tr("Doxygen Comment"), Qt::darkBlue));
    formatDescriptions.append(FormatDescription(QLatin1String(C_DOXYGEN_TAG), tr("Doxygen Tag"), Qt::blue));
    formatDescriptions.append(FormatDescription(QLatin1String(C_VISUAL_WHITESPACE), tr("Visual Whitespace"), Qt::lightGray));
    formatDescriptions.append(FormatDescription(QLatin1String(C_DISABLED_CODE), tr("Disabled Code")));

    // Diff categories
    formatDescriptions.append(FormatDescription(QLatin1String(C_ADDED_LINE), tr("Added Line"), QColor(0, 170, 0)));
    formatDescriptions.append(FormatDescription(QLatin1String(C_REMOVED_LINE), tr("Removed Line"), Qt::red));
    formatDescriptions.append(FormatDescription(QLatin1String(C_DIFF_FILE), tr("Diff File"), Qt::darkBlue));
    formatDescriptions.append(FormatDescription(QLatin1String(C_DIFF_LOCATION), tr("Diff Location"), Qt::blue));

    m_fontSettingsPage = new FontSettingsPage(formatDescriptions,
                                              QLatin1String("TextEditor"),
                                              tr("Text Editor"),
                                              this);
    pm->addObject(m_fontSettingsPage);

    // Add the GUI used to configure the tab, storage and interaction settings
    TextEditor::BehaviorSettingsPageParameters behaviorSettingsPageParameters;
    behaviorSettingsPageParameters.name = tr("Behavior");
    behaviorSettingsPageParameters.category = QLatin1String("TextEditor");
    behaviorSettingsPageParameters.trCategory = tr("Text Editor");
    behaviorSettingsPageParameters.settingsPrefix = QLatin1String("text");
    m_behaviorSettingsPage = new BehaviorSettingsPage(behaviorSettingsPageParameters, this);
    pm->addObject(m_behaviorSettingsPage);

    TextEditor::DisplaySettingsPageParameters displaySettingsPageParameters;
    displaySettingsPageParameters.name = tr("Display");
    displaySettingsPageParameters.category = QLatin1String("TextEditor");
    displaySettingsPageParameters.trCategory = tr("Text Editor");
    displaySettingsPageParameters.settingsPrefix = QLatin1String("text");
    m_displaySettingsPage = new DisplaySettingsPage(displaySettingsPageParameters, this);
    pm->addObject(m_displaySettingsPage);

    connect(m_fontSettingsPage, SIGNAL(changed(TextEditor::FontSettings)),
            this, SIGNAL(fontSettingsChanged(TextEditor::FontSettings)));
    connect(m_behaviorSettingsPage, SIGNAL(tabSettingsChanged(TextEditor::TabSettings)),
            this, SIGNAL(tabSettingsChanged(TextEditor::TabSettings)));
    connect(m_behaviorSettingsPage, SIGNAL(storageSettingsChanged(TextEditor::StorageSettings)),
            this, SIGNAL(storageSettingsChanged(TextEditor::StorageSettings)));
    connect(m_displaySettingsPage, SIGNAL(displaySettingsChanged(TextEditor::DisplaySettings)),
            this, SIGNAL(displaySettingsChanged(TextEditor::DisplaySettings)));
}

TextEditorSettings::~TextEditorSettings()
{
    ExtensionSystem::PluginManager *pm = ExtensionSystem::PluginManager::instance();
    pm->removeObject(m_fontSettingsPage);
    pm->removeObject(m_behaviorSettingsPage);
    pm->removeObject(m_displaySettingsPage);

    m_instance = 0;
}

TextEditorSettings *TextEditorSettings::instance()
{
    return m_instance;
}

/**
 * Initializes editor settings. Also connects signals to keep them up to date
 * when they are changed.
 */
void TextEditorSettings::initializeEditor(BaseTextEditor *editor)
{
    // Connect to settings change signals
    connect(this, SIGNAL(fontSettingsChanged(TextEditor::FontSettings)),
            editor, SLOT(setFontSettingsIfVisible(TextEditor::FontSettings)));
    connect(this, SIGNAL(tabSettingsChanged(TextEditor::TabSettings)),
            editor, SLOT(setTabSettings(TextEditor::TabSettings)));
    connect(this, SIGNAL(storageSettingsChanged(TextEditor::StorageSettings)),
            editor, SLOT(setStorageSettings(TextEditor::StorageSettings)));
    connect(this, SIGNAL(displaySettingsChanged(TextEditor::DisplaySettings)),
            editor, SLOT(setDisplaySettings(TextEditor::DisplaySettings)));

    connect(editor, SIGNAL(requestFontSize(int)),
            this, SLOT(fontSizeRequested(int)));

    // Apply current settings (tab settings depend on font settings)
    editor->setFontSettings(fontSettings());
    editor->setTabSettings(tabSettings());
    editor->setStorageSettings(storageSettings());
    editor->setDisplaySettings(displaySettings());
}


void TextEditorSettings::fontSizeRequested(int pointSize)
{
    FontSettings &fs = const_cast<FontSettings&>(m_fontSettingsPage->fontSettings());
    fs.setFontSize(pointSize);
    m_fontSettingsPage->saveSettings();
}

FontSettings TextEditorSettings::fontSettings() const
{
    return m_fontSettingsPage->fontSettings();
}

TabSettings TextEditorSettings::tabSettings() const
{
    return m_behaviorSettingsPage->tabSettings();
}

StorageSettings TextEditorSettings::storageSettings() const
{
    return m_behaviorSettingsPage->storageSettings();
}

DisplaySettings TextEditorSettings::displaySettings() const
{
    return m_displaySettingsPage->displaySettings();
}
