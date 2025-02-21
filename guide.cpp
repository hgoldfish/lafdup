#include "guide.h"
#include "ui_guide.h"

Guide::Guide(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Guide)
{
    ui->setupUi(this);
}

Guide::~Guide()
{
    delete ui;
}
