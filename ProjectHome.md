![http://beaverdbg.googlecode.com/files/beaverdbg.jpg](http://beaverdbg.googlecode.com/files/beaverdbg.jpg)

<font color='red'>Currently project is frozen and waits for a new enthusiastic maintainer</font>

Beaver Debugger is free cross-platform debugger (GDB frontend).

### Features ###
  * Supported C, C++ and QScript languages
  * Supported Unix/Linux, MS Windows, Mac OS and some other platforms.
  * Supports common debugging tasks, such as:
    1. Step through the program line‐by‐line or instruction‐by‐instruction
    1. Interrupt program execution
    1. Set breakpoints
    1. Examine call stack contents, watchers, and local and global variables
  * Has good looking visual interface.


It is an open source project, based on [Qt Creator](http://www.qtsoftware.com/products/developer-tools/developer-tools?currentflipperobject=821c7594d32e33932297b1e065a976b8) source code. Qt Creator's debugger itself based on [GNU Project Debugger](http://www.gnu.org/software/gdb)

Goal of the project is to create lightweight, universal, free, cross-platform debugger for C/C++ applications. Qt Creator already consists debugger, but it is a big IDE. This project provides only debugging functionality.

This project can be used as
  * Standalone C/C++ debugger. Now exists few free visual debuggers, which can be used under Unix/Linux. Check this list of the GDB frontends [list](http://en.wikipedia.org/wiki/Debugger_front-end). This project intended to be one of them. But have important advantages compared with others.
  * Integrated with an IDE.

Screenshot:

[![](http://beaverdbg.googlecode.com/files/beaverdbg-main-ui-little.png)](http://code.google.com/p/beaverdbg/wiki/ScreenShot)
### TODO ###
  * Make sure all Qt Creator features supported (console output of the target available, debugger supports Qt types via gdb macroses, ...)
  * Move to the latest Qt Creator code base

### Authors ###
  * Andrei Kopats aka hlamer supported by [Monkey Studio Team](http://monkeystudio.org).Created patch for the Qt Creator.
  * Filipe Azevedo aka P@sNox. Made some build system fixes, Mac OS X packager.
  * Evgeni Golov aka Zhenech. Debian packager.
  * Designer. Author of the original logo. Name currently is unknown.


Use an email address [mailto:hlamer@tut.by](mailto:hlamer@tut.by), or irc channel [#monkeystudio](http://monkeystudio.org/irc) on irc.freenode.net for contact.

### Development details ###
#### Version control ####
Since version 1.0.0.a1 project source code is hosted on Google Code SVN. See Sources tab.
Older (development) versions are on Monkey Studio SVN, and not imported.
If you need check older versions, use
url: `svn//svn.tuxfamily.org/svnroot/monkeystudio/mks/beaver` [revision 3289](https://code.google.com/p/beaverdbg/source/detail?r=3289)

[Help with the MkS subversion repository](http://monkeystudio.org/node/11)

#### New features development ####
If will be necessary to develop new debugging features, preferred way is to add it at first at the Qt Creator, than update Beaver Debugger.

#### Coding style ####
Use please Qt Creator original coding style

#### Version numeration ####
Beaver version will be the same as base Qt Creator source code version.

Unstable version can be suffixed with letter 'a' - alpha, 'b' - beta, and unstable version number.
For example, the first release would have version number 1.0.0.a1

### Support this project ###
#### User ####
Send your response and bug reports, it would be usefull
#### Developer ####
New developers, and distribution package maintainers are welcomed
#### Sponsor ####
Use [Monkey Studio donation page](https://sourceforge.net/donate/index.php?group_id=163493)

Write please in the comment, that it is donation for the "Beaver Debugger"

<a href='http://www.brothersoft.com/beaver-debugger-299685.htm'><img src='http://author.brothersoft.com/softimg/pick_100.gif' alt='Beaverdbg on BrotherSoft' height='70' /></a>