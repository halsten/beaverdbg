<plugin name="Debugger" version="1.3.1" compatVersion="1.3.1">
    <vendor>Nokia Corporation</vendor>
    <copyright>(C) 2008-2009 Nokia Corporation</copyright>
    <license>
Commercial Usage

Licensees holding valid Qt Commercial licenses may use this plugin in
accordance with the Qt Commercial License Agreement provided with the
Software or, alternatively, in accordance with the terms contained in
a written agreement between you and Nokia.

GNU Lesser General Public License Usage

Alternatively, this plugin may be used under the terms of the GNU Lesser
General Public License version 2.1 as published by the Free Software
Foundation.  Please review the following information to
ensure the GNU Lesser General Public License version 2.1 requirements
will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.</license>
    <description>Debugger integration.</description>
    <url>http://qt.nokia.com</url>
    <dependencyList>
        <dependency name="CppEditor" version="1.3.1"/><!-- Debugger plugin adds items to the editor's context menu -->
        <dependency name="ProjectExplorer" version="1.3.1"/>
        <dependency name="Core" version="1.3.1"/>
        <dependency name="Find" version="1.3.1"/>
    </dependencyList>
    <argumentList>
        <argument name="-disable-cdb">Disable Cdb debugger engine</argument>
        <argument name="-disable-gdb">Disable Gdb debugger engine</argument>
        <argument name="-disable-sdb">Disable Qt Script debugger engine</argument>
        <argument name="-debug" parameter="pid-or-corefile">Attach to Process-Id or Core file</argument>
        <argument name="-wincrashevent" parameter="event-handle">Event handle used for attaching to crashed processes</argument>
    </argumentList>
</plugin>
