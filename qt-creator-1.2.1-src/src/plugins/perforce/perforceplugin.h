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

#ifndef PERFORCEPLUGIN_H
#define PERFORCEPLUGIN_H

#include "perforcesettings.h"

#include <coreplugin/editormanager/ieditorfactory.h>
#include <coreplugin/iversioncontrol.h>
#include <coreplugin/icorelistener.h>
#include <extensionsystem/iplugin.h>
#include <projectexplorer/projectexplorer.h>

#include <QtCore/QObject>
#include <QtCore/QProcess>
#include <QtCore/QStringList>

QT_BEGIN_NAMESPACE
class QFile;
class QAction;
class QTemporaryFile;
class QTextCodec;
QT_END_NAMESPACE

namespace Core {
    class IEditorFactory;
    namespace Utils {
        class ParameterAction;
    }
}

namespace Perforce {
namespace Internal {

class PerforceOutputWindow;
class SettingsPage;
class PerforceVersionControl;
class PerforcePlugin;

// Just a proxy for PerforcePlugin
class CoreListener : public Core::ICoreListener
{
    Q_OBJECT
public:
    CoreListener(PerforcePlugin *plugin) : m_plugin(plugin) { }
    bool editorAboutToClose(Core::IEditor *editor);
    bool coreAboutToClose() { return true; }
private:
    PerforcePlugin *m_plugin;
};

struct PerforceResponse
{
    bool error;
    QString stdOut;
    QString stdErr;
    QString message;
};

class PerforcePlugin : public ExtensionSystem::IPlugin
{
    Q_OBJECT

public:
    PerforcePlugin();
    ~PerforcePlugin();

    SettingsPage *settingsPage() const { return m_settingsPage; }

    bool initialize(const QStringList &arguments, QString *error_message);
    void extensionsInitialized();

    bool managesDirectory(const QString &directory) const;
    QString findTopLevelForDirectory(const QString &directory) const;
    bool vcsOpen(const QString &fileName);
    bool vcsAdd(const QString &fileName);
    bool vcsDelete(const QString &filename);
    // Displays the message for the submit mesage
    bool editorAboutToClose(Core::IEditor *editor);

    void p4Diff(const QStringList &files, QString diffname = QString());

    Core::IEditor *openPerforceSubmitEditor(const QString &fileName, const QStringList &depotFileNames);

    static PerforcePlugin *perforcePluginInstance();

    const PerforceSettings& settings() const;
    void setSettings(const Settings &s);

    // Map a perforce name "//xx" to its real name in the file system
    QString fileNameFromPerforceName(const QString& perforceName, QString *errorMessage) const;

public slots:
    void describe(const QString &source, const QString &n);

private slots:;
    void openCurrentFile();
    void addCurrentFile();
    void deleteCurrentFile();
    void revertCurrentFile();
    void printOpenedFileList();
    void diffCurrentFile();
    void diffCurrentProject();
    void diffAllOpened();
    void submit();
    void describeChange();
    void annotateCurrentFile();
    void annotate();
    void filelogCurrentFile();
    void filelog();

    void updateActions();
    void submitCurrentLog();
    void printPendingChanges();
    void slotDiff(const QStringList &files);

private:
    QStringList environment() const;

    Core::IEditor *showOutputInEditor(const QString& title, const QString output,
                                      int editorType,
                                      QTextCodec *codec = 0);

    // Verbosity flags for runP4Cmd.
    enum  RunLogFlags { CommandToWindow = 0x1, StdOutToWindow = 0x2, StdErrToWindow = 0x4, ErrorToWindow = 0x8 };

    // args are passed as command line arguments
    // extra args via a tempfile and the option -x "temp-filename"
    PerforceResponse runP4Cmd(const QStringList &args,
                              const QStringList &extraArgs = QStringList(),
                              unsigned logFlags = CommandToWindow|StdErrToWindow|ErrorToWindow,
                              QTextCodec *outputCodec = 0) const;

    void openFiles(const QStringList &files);

    QString clientFilePath(const QString &serverFilePath);
    QString currentFileName();
    bool checkP4Configuration(QString *errorMessage = 0) const;
    void showOutput(const QString &output, bool popup = false) const;
    void annotate(const QString &fileName);
    void filelog(const QString &fileName);
    void cleanChangeTmpFile();

    ProjectExplorer::ProjectExplorerPlugin *m_projectExplorer;
    PerforceOutputWindow *m_perforceOutputWindow;
    SettingsPage *m_settingsPage;
    QList<Core::IEditorFactory*> m_editorFactories;

    Core::Utils::ParameterAction *m_editAction;
    Core::Utils::ParameterAction *m_addAction;
    Core::Utils::ParameterAction *m_deleteAction;
    QAction *m_openedAction;
    Core::Utils::ParameterAction *m_revertAction;
    Core::Utils::ParameterAction *m_diffCurrentAction;
    Core::Utils::ParameterAction *m_diffProjectAction;
    QAction *m_diffAllAction;
    QAction *m_resolveAction;
    QAction *m_submitAction;
    QAction *m_pendingAction;
    QAction *m_describeAction;
    Core::Utils::ParameterAction *m_annotateCurrentAction;
    QAction *m_annotateAction;
    Core::Utils::ParameterAction *m_filelogCurrentAction;
    QAction *m_filelogAction;
    QAction *m_submitCurrentLogAction;
    bool m_submitActionTriggered;
    QAction *m_diffSelectedFiles;

    QAction *m_undoAction;
    QAction *m_redoAction;

    QTemporaryFile *m_changeTmpFile;

    static const char * const PERFORCE_MENU;
    static const char * const EDIT;
    static const char * const ADD;
    static const char * const DELETE_FILE;
    static const char * const OPENED;
    static const char * const REVERT;
    static const char * const DIFF_ALL;
    static const char * const DIFF_PROJECT;
    static const char * const DIFF_CURRENT;
    static const char * const RESOLVE;
    static const char * const SUBMIT;
    static const char * const PENDING_CHANGES;
    static const char * const DESCRIBE;
    static const char * const ANNOTATE_CURRENT;
    static const char * const ANNOTATE;
    static const char * const FILELOG_CURRENT;
    static const char * const FILELOG;
    static const char * const SEPARATOR1;
    static const char * const SEPARATOR2;
    static const char * const SEPARATOR3;

    static PerforcePlugin *m_perforcePluginInstance;
    QString pendingChangesData();

    CoreListener *m_coreListener;
    Core::IEditorFactory *m_submitEditorFactory;
    PerforceVersionControl *m_versionControl;
    PerforceSettings m_settings;

    friend class PerforceOutputWindow; // needs openFiles()
};

} // namespace Perforce
} // namespace Internal

#endif // PERFORCEPLUGIN_H
