#ifndef GUIDE_H
#define GUIDE_H

#include <QDialog>

namespace Ui {
class GuideDialog;
}

class GuideDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GuideDialog(QWidget *parent = nullptr);
    ~GuideDialog();
private:
    Ui::GuideDialog *ui;
};

#endif  // GUIDE_H
