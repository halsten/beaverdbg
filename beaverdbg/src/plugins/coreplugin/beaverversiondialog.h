#ifndef BEAVERVERSIONDIALOG_H
#define BEAVERVERSIONDIALOG_H

#include <QtGui/QDialog>

namespace Core {
namespace Internal {

class BeaverVersionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BeaverVersionDialog(QWidget *parent);
};

} // namespace Internal
} // namespace Core

#endif // BEAVERVERSIONDIALOG_H
