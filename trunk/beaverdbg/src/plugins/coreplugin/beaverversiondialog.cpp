#include "beaverversiondialog.h"

#include "coreconstants.h"
#include "icore.h"

#include <utils/qtcassert.h>

#include <QtCore/QDate>
#include <QtCore/QFile>

#include <QtGui/QDialogButtonBox>
#include <QtGui/QGridLayout>
#include <QtGui/QLabel>
#include <QtGui/QPushButton>
#include <QtGui/QTextBrowser>

using namespace Core;
using namespace Core::Internal;
using namespace Core::Constants;

BeaverVersionDialog::BeaverVersionDialog(QWidget *parent)
    : QDialog(parent)
{
    // We need to set the window icon explicitly here since for some reason the
    // application icon isn't used when the size of the dialog is fixed (at least not on X11/GNOME)
    setWindowIcon(QIcon(":/core/images/qtcreator_logo_128.png"));

    setWindowTitle(tr("About Beaver"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    QGridLayout *layout = new QGridLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    QString version = QLatin1String(IDE_VERSION_LONG);
    version += QDate(2007, 25, 10).toString(Qt::SystemLocaleDate);

    const QString description = tr(
        "<h3>Beaver %1</h3>"
        "Based on Qt Creator %2<br/>"
        "<br/>"
        "Built on " __DATE__ " at " __TIME__ "<br />"
#ifdef IDE_REVISION
        "From revision %5<br/>"
#endif
        "<br/>"
        "Beaver is <a href='http://www.qtsoftware.com/products/appdev/developer-tools/developer-tools#qt-tools-at-a'>Qt Creator</a> adaptation for usage as stand alone debugger.<br/>"
        "Done by Andrei Kopats aka hlamer and <a href='http://www.monkeystudio.org'>Monkey Studio team.</a><br/>"
        "See <a href='http://www.monkeystudio.org'>TODO project home page</a> for additional info.<br/>"
        "For more information about original Qt Creator see <i>Help->About original Qt Creator</i> dialog<br/>"
        "<br/>"
        "You could report bugs on <a href='http://www.monkeystudio.org/node/24'>this page.</a><br/>"
        "Use IRC channel <i>#monkeystudio</i> on <i>irc.freenode.net</i>, or <a href='http://www.monkeystudio.org/irc'>this page</a> for interactive contact with the developers<br/>"
        "<br/>"
        "Copyright 2008-%3 %4.<br/>"
        "License: LGPL v2.1<br/>"
        "<br/>"
        "The program is provided AS IS with NO WARRANTY OF ANY KIND, "
        "INCLUDING THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A "
        "PARTICULAR PURPOSE.<br/>")
        .arg("0.1a", version, "2009", "Andrei Kopats and Monkey Studio team"
#ifdef IDE_REVISION
             , QString(IDE_REVISION_STR).left(10)
#endif
             );

    QLabel *copyRightLabel = new QLabel(description);
    copyRightLabel->setWordWrap(true);
    copyRightLabel->setOpenExternalLinks(true);
    copyRightLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    QPushButton *closeButton = buttonBox->button(QDialogButtonBox::Close);
    QTC_ASSERT(closeButton, /**/);
    buttonBox->addButton(closeButton, QDialogButtonBox::ButtonRole(QDialogButtonBox::RejectRole | QDialogButtonBox::AcceptRole));
    connect(buttonBox , SIGNAL(rejected()), this, SLOT(reject()));

    QLabel *logoLabel = new QLabel;
    logoLabel->setPixmap(QPixmap(QLatin1String(":/core/images/qtcreator_logo_128.png")));
    layout->addWidget(logoLabel , 0, 0, 1, 1);
    layout->addWidget(copyRightLabel, 0, 1, 4, 4);
    layout->addWidget(buttonBox, 4, 0, 1, 5);
}
