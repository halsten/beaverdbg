#include <QApplication>
#include <QMainWindow>
#include <QStatusBar>
#include <QLabel>
#include <QStringList>
#include <QProcess>
#include <QFileInfo>
#include <QDebug>

#include <extensionsystem/pluginmanager.h>
#include <iplugin.h>

#include <debuggermanager.h>
#include <debuggerplugin.h>

#include "mainwindow.h"
#include "modemanager.h"
#include "projectexplorer/projectexplorer.h"
#include "coreplugin/coreplugin.h"
#include "find/findplugin.h"
#include "quickopen/quickopenplugin.h"
#include "texteditor/texteditorplugin.h"
#include "cppeditor/cppplugin.h"

static void print_help()
{
	qWarning("Usage: beaver PROGRAMM ARGUMENTS");
	qWarning("\nTODO: possibility to attach to running process, remote debugging, transcript file");
	qWarning("	waits for volunteer");
}

int main(int argc, char **argv)
{	
	QApplication app (argc, argv);
	
	QStringList args;
	for (int i = 1; i < argc; i++)
	{
		args << argv[i];
	}
	
	QString targetFileName;
	if (args.size() == 1 &&
		(args[0] == "-h" ||
		 args[0] == "--help"))
	{
		print_help();
		return 0;
	}
	
	if (args.size() > 0)
	{
		QStringList path;
		path << "" << ".";

		QStringList env = QProcess::systemEnvironment();
		foreach(QString var, env)
		{
			if (var.startsWith("PATH="))
			{
				var.remove(0, 5);
#ifdef Q_OS_WIN
				QStringList pathDirs = var.split(";");
#else
				QStringList pathDirs = var.split(":");
#endif
				path.append(pathDirs);
			}
		}
		bool found  = false;
		foreach(QString dir, path)
		{
			targetFileName = dir + "/" + args[0];
			QFileInfo info(targetFileName);
			if (info.isFile() && info.isExecutable())
			{
				found = true;
				break;
			}
		}
		if (found)
		{
			args.removeAt(0);
		}
		else
		{
			qWarning() << args[0] << "is not a valid executable file";
			print_help();
			return -1;
		}
	}
	
	ExtensionSystem::PluginManager pluginManager;
	// for create instance
	QString error;
	
	Core::Internal::CorePlugin *core = new Core::Internal::CorePlugin();
	ProjectExplorer::ProjectExplorerPlugin *projectExplorer = new ProjectExplorer::ProjectExplorerPlugin();
	Debugger::Internal::DebuggerPlugin *debugger = new Debugger::Internal::DebuggerPlugin();
	Find::Internal::FindPlugin *find = new Find::Internal::FindPlugin();
	//QuickOpen::Internal::QuickOpenPlugin quickOpen = new QuickOpen::Internal::QuickOpenPlugin();
	TextEditor::Internal::TextEditorPlugin *textEditor = new TextEditor::Internal::TextEditorPlugin();
	CppEditor::Internal::CppPlugin *cpp = new CppEditor::Internal::CppPlugin();
	
	core->initialize (QStringList(), &error);
	projectExplorer->initialize(QStringList(), &error);
	ExtensionSystem::IPlugin* i_debugger_plugin = static_cast<ExtensionSystem::IPlugin*>(debugger);
	i_debugger_plugin->initialize (QStringList(), &error);	
	find->initialize (QStringList(), &error);
	//quickOpen.initialize (QStringList(), &error);
	textEditor->initialize (QStringList(), &error);
	cpp->initialize (QStringList(), &error);
	
	core->extensionsInitialized();
	
	if (! targetFileName.isEmpty())
	{
		debugger->startNewDebugger(targetFileName, args);
	}
	int res = app.exec();
	
	delete cpp;
	delete textEditor;
	//delete quickOpen;
	delete find;
	i_debugger_plugin->shutdown();
	delete debugger;
	delete projectExplorer;
	delete core;
	
	return res;
}
