#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "callwindow.h"


#include <QHostAddress>
#include <QtEndian>


MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui_(new Ui::MainWindow),
  call_(NULL)
{
  ui_->setupUi(this);
}

MainWindow::~MainWindow()
{
  delete ui_;
  ui_ = 0;
  delete call_;
  call_ = 0;
}

void MainWindow::startCall()
{
  if(call_)
  {
    delete call_;
    call_ = NULL;
  }

  QString width = ui_->width->toPlainText();
  QString height = ui_->height->toPlainText();

  call_ = new CallWindow(this, width.toInt(), height.toInt());
  call_->show();
  call_->startStream();
}


