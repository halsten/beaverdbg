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

#include "fakevimhandler.h"

#include <QtCore/QSet>

#include <QtGui/QTextEdit>
#include <QtGui/QPlainTextEdit>

#include <QtTest/QtTest>

using namespace FakeVim;
using namespace FakeVim::Internal;

#define EDITOR(s) (m_textedit ? m_textedit->s : m_plaintextedit->s)

class tst_FakeVim : public QObject
{
    Q_OBJECT

public:
    tst_FakeVim(bool);
    ~tst_FakeVim();

public slots:
    void changeStatusData(const QString &info) { m_statusData = info; }
    void changeStatusMessage(const QString &info) { m_statusMessage = info; }
    void changeExtraInformation(const QString &info) { m_infoMessage = info; }
    
private slots:
    // command mode
    void command_cc();
    void command_dd();
    void command_dollar();
    void command_down();
    void command_dfx_down();
    void command_Cxx_down_dot();
    void command_e();
    void command_i();
    void command_left();
    void command_r();
    void command_right();
    void command_up();
    void command_w();

    // special tests
    void test_i_cw_i();

private:
    void setup();    
    void send(const QString &command) { sendEx("normal " + command); }
    void sendEx(const QString &command); // send an ex command

    bool checkContentsHelper(QString expected, const char* file, int line);
    bool checkHelper(bool isExCommand, QString cmd, QString expected,
        const char* file, int line);
    QString insertCursor(const QString &needle0);

    QString lmid(int i, int n = -1) const
        { return QStringList(l.mid(i, n)).join("\n"); }

    QTextEdit *m_textedit;
    QPlainTextEdit *m_plaintextedit;
    FakeVimHandler *m_handler;
    QList<QTextEdit::ExtraSelection> m_selection;

    QString m_statusMessage;
    QString m_statusData;
    QString m_infoMessage;

    // the individual lines
    static const QStringList l; // identifier intentionally kept short
    static const QString lines;
    static const QString escape;
};

const QString tst_FakeVim::lines = 
  /* 0         1         2         3        4 */
  /* 0123456789012345678901234567890123457890 */
    "\n"
    "#include <QtCore>\n"
    "#include <QtGui>\n"
    "\n"
    "int main(int argc, char *argv[])\n"
    "{\n"
    "    QApplication app(argc, argv);\n"
    "\n"
    "    return app.exec();\n"
    "}\n";

const QStringList tst_FakeVim::l = tst_FakeVim::lines.split('\n');

const QString tst_FakeVim::escape = QChar(27);

QString control(int c)
{
    return QChar(c + 256);
}


tst_FakeVim::tst_FakeVim(bool usePlainTextEdit)
{
    if (usePlainTextEdit) {
        m_textedit = 0;
        m_plaintextedit = new QPlainTextEdit;
    } else {
        m_textedit = new QTextEdit;
        m_plaintextedit = 0;
    }
    m_handler = 0;
}

tst_FakeVim::~tst_FakeVim()
{
    delete m_handler;
    delete m_textedit;
    delete m_plaintextedit;
}

void tst_FakeVim::setup()
{
    delete m_handler;
    m_handler = 0;
    m_statusMessage.clear();
    m_statusData.clear();
    m_infoMessage.clear();
    if (m_textedit) {
        m_textedit->setPlainText(lines);
        QTextCursor tc = m_textedit->textCursor();
        tc.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
        m_textedit->setTextCursor(tc);
        m_textedit->setPlainText(lines);
        m_handler = new FakeVimHandler(m_textedit);
    } else {
        m_plaintextedit->setPlainText(lines);
        QTextCursor tc = m_plaintextedit->textCursor();
        tc.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
        m_plaintextedit->setTextCursor(tc);
        m_plaintextedit->setPlainText(lines);
        m_handler = new FakeVimHandler(m_plaintextedit);
    }

    QObject::connect(m_handler, SIGNAL(commandBufferChanged(QString)),
        this, SLOT(changeStatusMessage(QString)));
    QObject::connect(m_handler, SIGNAL(extraInformationChanged(QString)),
        this, SLOT(changeExtraInformation(QString)));
    QObject::connect(m_handler, SIGNAL(statusDataChanged(QString)),
        this, SLOT(changeStatusData(QString)));

    QCOMPARE(EDITOR(toPlainText()), lines);
}

void tst_FakeVim::sendEx(const QString &command)
{
    if (m_handler)
        m_handler->handleCommand(command);
    else
        qDebug() << "NO HANDLER YET";
}

bool tst_FakeVim::checkContentsHelper(QString want, const char* file, int line)
{
    QString got = EDITOR(toPlainText());
    int pos = EDITOR(textCursor().position());
    got = got.left(pos) + "@" + got.mid(pos);
    QStringList wantlist = want.split('\n');
    QStringList gotlist = got.split('\n');
    if (!QTest::qCompare(gotlist.size(), wantlist.size(), "", "", file, line)) {
        qDebug() << "0 WANT: " << want;
        qDebug() << "0 GOT: " << got;
        return false;
    }
    for (int i = 0; i < wantlist.size() && i < gotlist.size(); ++i) {
        QString g = QString("line %1: %2").arg(i + 1).arg(gotlist.at(i));
        QString w = QString("line %1: %2").arg(i + 1).arg(wantlist.at(i));
        if (!QTest::qCompare(g, w, "", "", file, line)) {
            qDebug() << "1 WANT: " << want;
            qDebug() << "1 GOT: " << got;
            return false;
        }
    }
    return true;
}

bool tst_FakeVim::checkHelper(bool ex, QString cmd, QString expected,
    const char *file, int line)
{
    if (ex)
        sendEx(cmd);
    else
        send(cmd);
    return checkContentsHelper(expected, file, line);
}


#define checkContents(expected) \
    do { if (!checkContentsHelper(expected, __FILE__, __LINE__)) return; } while (0)

// Runs a "normal" command and checks the result.
// Cursor position is marked by a '@' in the expected contents.
#define check(cmd, expected) \
    do { if (!checkHelper(false, cmd, expected, __FILE__, __LINE__)) \
            return; } while (0)

#define move(cmd, expected) \
    do { if (!checkHelper(false, cmd, insertCursor(expected), __FILE__, __LINE__)) \
            return; } while (0)

// Runs an ex command and checks the result.
// Cursor position is marked by a '@' in the expected contents.
#define checkEx(cmd, expected) \
    do { if (!checkHelper(true, cmd, expected, __FILE__, __LINE__)) \
            return; } while (0)

QString tst_FakeVim::insertCursor(const QString &needle0)
{
    QString needle = needle0;
    needle.remove('@');
    QString lines0 = lines;
    int pos = lines0.indexOf(needle);
    if (pos == -1)
        qDebug() << "Cannot find: \n----\n" + needle + "\n----\n";
    lines0.replace(pos, needle.size(), needle0);
    return lines0;
}


//////////////////////////////////////////////////////////////////////////
//
// Command mode
//
//////////////////////////////////////////////////////////////////////////

void tst_FakeVim::command_cc()
{
    setup();
    move("j",                "@" + l[1]);
    check("ccabc" + escape,  l[0] + "\nab@c\n" + lmid(2));
    check("ccabc" + escape,  l[0] + "\nab@c\n" + lmid(2));
    check(".",               l[0] + "\nab@c\n" + lmid(2));
    check("j",               l[0] + "\nabc\n#i@nclude <QtGui>\n" + lmid(3));
    check("3ccxyz" + escape, l[0] + "\nabc\nxy@z\n" + lmid(5));
}

void tst_FakeVim::command_dd()
{
    setup();
    move("j",    "@" + l[1]);
    check("dd",  l[0] + "\n@" + lmid(2));
    check(".",   l[0] + "\n@" + lmid(3));
    check("3dd", l[0] + "\n@" + lmid(6));
    check("8l",  l[0] + "\n    QApp@lication app(argc, argv);\n" + lmid(7));
    check("dd",  l[0] + "\n@" + lmid(7));
    check(".",   l[0] + "\n@" + lmid(8));
    check("dd",  l[0] + "\n@" + lmid(9));
}

void tst_FakeVim::command_dollar()
{
    setup();
    move("j$", "<QtCore>@");
    move("j$", "<QtGui>@");
    move("2j", ")@");
}

void tst_FakeVim::command_down()
{
    setup();
    move("j",  "@" + l[1]);
    move("3j", "@int main");
    move("4j", "@    return app.exec()");
}

void tst_FakeVim::command_dfx_down()
{
    setup();
    check("j4l",  l[0] + "\n#inc@lude <QtCore>\n" + lmid(2));
    check("df ",  l[0] + "\n#inc@<QtCore>\n" + lmid(2));
    check("j",    l[0] + "\n#inc<QtCore>\n#inc@lude <QtGui>\n" + lmid(3));
    check(".",    l[0] + "\n#inc<QtCore>\n#inc@<QtGui>\n" + lmid(3));
return;
    check("u",    l[0] + "\n#inc<QtCore>\n#inc@lude <QtGui>\n" + lmid(3));
    check("u",    l[0] + "\n#inc@lude <QtCore>\n" + lmid(2));
}

void tst_FakeVim::command_Cxx_down_dot()
{
    setup();
    check("j4l",          l[0] + "\n#inc@lude <QtCore>\n" + lmid(2));
    check("Cxx" + escape, l[0] + "\n#incx@x\n" + lmid(2));
    check("j",            l[0] + "\n#incxx\n#incl@ude <QtGui>\n" + lmid(3));
    check(".",            l[0] + "\n#incxx\n#inclx@x\n" + lmid(3));
}

void tst_FakeVim::command_e()
{
    setup();
    move("e",  "@#include <QtCore");
    move("e",  "#includ@e <QtCore");
    move("e",  "#include @<QtCore");
    move("3e", "@#include <QtGui");
    move("e",  "#includ@e <QtGui");
    move("e",  "#include @<QtGui");
    move("e",  "#include <QtGu@i");
    move("4e", "int main@(int argc, char *argv[])");
    move("e",  "int main(in@t argc, char *argv[])");
    move("e",  "int main(int arg@c, char *argv[])");
    move("e",  "int main(int argc@, char *argv[])");
    move("e",  "int main(int argc, cha@r *argv[])");
    move("e",  "int main(int argc, char @*argv[])");
    move("e",  "int main(int argc, char *arg@v[])");
    move("e",  "int main(int argc, char *argv[]@)");
    move("e",  "@{");
    move("10k","@\n"); // home.
}

void tst_FakeVim::command_i()
{
    setup();

    // empty insertion at start of document
    check("i" + escape, "@" + lines);
    check("u", "@" + lines);

    // small insertion at start of document
    check("ix" + escape, "@x" + lines);
    check("u", "@" + lines);
// FIXME redo broken
    //check(control('r'), "@x" + lines);
    //check("u", "@" + lines);

    // small insertion at start of document
    check("ixxx" + escape, "xx@x" + lines);
    check("u", "@" + lines);

    // combine insertions
    check("i1" + escape, "@1" + lines);
    check("i2" + escape, "@21" + lines);
    check("i3" + escape, "@321" + lines);
    check("u",           "@21" + lines);
    check("u",           "@1" + lines);
    check("u",           "@" + lines);
    check("ia" + escape, "@a" + lines);
    check("ibx" + escape, "b@xa" + lines);
    check("icyy" + escape, "bcy@yxa" + lines);
    check("u", "b@xa" + lines);
    check("u", "@a" + lines); 
// FIXME undo broken
//    checkEx("redo", "b@xa" + lines);
//    check("u", "@a" + lines);
}

void tst_FakeVim::command_left()
{
    setup();
    move("4j",  "@int main");
    move("h",   "@int main"); // no move over left border
    move("$",   "argv[])@");
    move("h",   "argv[]@)");
    move("3h",  "arg@v[])");
    move("50h", "@int main");
}

void tst_FakeVim::command_r()
{
    setup();
    move("4j",   "@int main");
    move("$",    "int main(int argc, char *argv[])@");
    check("rx",  lmid(0, 4) + "\nint main(int argc, char *argv[]x@\n" + lmid(5)); 
    check("2h",  lmid(0, 4) + "\nint main(int argc, char *argv[@]x\n" + lmid(5));
    check("4ra", lmid(0, 4) + "\nint main(int argc, char *argv[@]x\n" + lmid(5));
return; // FIXME
    check("3rb", lmid(0, 4) + "\nint main(int argc, char *argv[bb@b\n" + lmid(5));
    check("2rc", lmid(0, 4) + "\nint main(int argc, char *argv[bb@b\n" + lmid(5));
    check("h2rc",lmid(0, 4) + "\nint main(int argc, char *argv[bc@c\n" + lmid(5));
}

void tst_FakeVim::command_right()
{
    setup();
    move("4j", "@int main");
    move("l", "i@nt main");
    move("3l", "int @main");
    move("50l", "argv[])@");
}

void tst_FakeVim::command_up()
{
    setup();
    move("j", "@#include <QtCore");
    move("3j", "@int main");
    move("4j", "@    return app.exec()");
}

void tst_FakeVim::command_w()
{
    setup();
    move("w",   "@#include <QtCore");
    move("w",   "#@include <QtCore");
    move("w",   "#include @<QtCore");
    move("3w",  "@#include <QtGui");
    move("w",   "#@include <QtGui");
    move("w",   "#include @<QtGui");
    move("w",   "#include <@QtGui");
    move("4w",  "int main@(int argc, char *argv[])");
    move("w",   "int main(@int argc, char *argv[])");
    move("w",   "int main(int @argc, char *argv[])");
    move("w",   "int main(int argc@, char *argv[])");
    move("w",   "int main(int argc, @char *argv[])");
    move("w",   "int main(int argc, char @*argv[])");
    move("w",   "int main(int argc, char *@argv[])");
    move("w",   "int main(int argc, char *argv@[])");
    move("w",   "@{");
}

/*
#include <QtCore>
#include <QtGui>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    return app.exec();
}
*/


void tst_FakeVim::test_i_cw_i()
{
    setup();
    move("j",                "@" + l[1]);
    check("ixx" + escape,    l[0] + "\nx@x" + lmid(1));
return; // FIXME: not in sync with Gui behaviour?
    check("cwyy" + escape,   l[0] + "\nxy@y" + lmid(1));
    check("iaa" + escape,    l[0] + "\nxya@ay" + lmid(1));
}


//////////////////////////////////////////////////////////////////////////
//
// Main
//
//////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) \
{
    int res = 0;
    QApplication app(argc, argv); \

    // Test with QPlainTextEdit.
    tst_FakeVim plaintextedit(true);
    res += QTest::qExec(&plaintextedit, argc, argv);

#if 0
    // Test with QTextEdit, too.
    tst_FakeVim textedit(false);
    res += QTest::qExec(&textedit, argc, argv);
#endif

    return res;
}


#include "main.moc"
