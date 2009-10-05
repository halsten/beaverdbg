#ifndef ADDRESSBOOK_H
#define ADDRESSBOOK_H

#include <QtGui/QWidget>
#include <QtGui/QPushButton>
#include <QtGui/QLineEdit>
#include <QtGui/QTextEdit>
#include <QtGui/QMessageBox>


namespace Ui
{
    class AddressBook;
}

class AddressBook : public QWidget
{
    Q_OBJECT

public:
    AddressBook(QWidget *parent = 0);
//! [enum]
    enum Mode { NavigationMode, AddingMode, EditingMode };
//! [enum]
    ~AddressBook();

public slots:
    void addContact();
    void submitContact();
    void cancel();
//! [slot definition]
    void editContact();
    void removeContact();
//! [slot definition]
    void next();
    void previous();

private:
    Ui::AddressBook *ui;

//! [updateInterface]
    void updateInterface(Mode mode);
//! [updateInterface]
    QPushButton *addButton;
    QPushButton *submitButton;
    QPushButton *cancelButton;
//! [members]
    QPushButton *editButton;
    QPushButton *removeButton;
//! [members]
    QPushButton *nextButton;
    QPushButton *previousButton;
    QLineEdit *nameLine;
    QTextEdit *addressText;

    QMap<QString, QString> contacts;
    QString oldName;
    QString oldAddress;
//! [current mode]
    Mode currentMode;
//! [current mode]
};

#endif // ADDRESSBOOK_H
