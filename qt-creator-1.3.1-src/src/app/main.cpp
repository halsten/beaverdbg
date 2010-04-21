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
#include "texteditor/texteditorplugin.h"
#include "cppeditor/cppplugin.h"

static void print_help(char* name)
{
	qWarning("Usage:");
	qWarning("    %s [OPTIONS ] [PROGRAMM [ ARGUMENTS ]]", name);
	qWarning("Options:");
	qWarning("  -v | --version           Print programm version");
	qWarning("  -h | --help              Print this help");
	qWarning("\nTODO: Possibility to attach to running process, remote debugging, transcript file");
	qWarning("      waits for volunteer");
}

static void print_version()
{
	qWarning("Beaver Debugger version 1.0.0.b2");
	qWarning("www.beaverdbg.googlecode.com");
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
		print_help(argv[0]);
		return 0;
	}
	
	if (args.size() == 1 &&
		(args[0] == "-v" ||
		 args[0] == "--version"))
	{
		print_version();
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
			print_help(argv[0]);
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
	TextEditor::Internal::TextEditorPlugin *textEditor = new TextEditor::Internal::TextEditorPlugin();
	CppEditor::Internal::CppPlugin *cpp = new CppEditor::Internal::CppPlugin();
	
	core->initialize (QStringList(), &error);
	projectExplorer->initialize(QStringList(), &error);
	ExtensionSystem::IPlugin* i_debugger_plugin = static_cast<ExtensionSystem::IPlugin*>(debugger);
	i_debugger_plugin->initialize (QStringList(), &error);	
	find->initialize (QStringList(), &error);
	textEditor->initialize (QStringList(), &error);
	cpp->initialize (QStringList(), &error);
	
	core->extensionsInitialized();
	
	if (! targetFileName.isEmpty())
	{
#if 0
		debugger->startNewDebugger(targetFileName, args);
#endif
	}
	int res = app.exec();
	
	delete cpp;
	delete textEditor;
	delete find;
	i_debugger_plugin->shutdown();
	delete debugger;
	delete projectExplorer;
	delete core;
	
	return res;
}
