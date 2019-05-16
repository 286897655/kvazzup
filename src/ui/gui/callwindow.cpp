#include "callwindow.h"

#include "ui_callwindow.h"
#include "ui_about.h"

#include "statisticswindow.h"
#include "videoviewfactory.h"

#include <QCloseEvent>
#include <QTimer>
#include <QMetaType>
#include <QDebug>
#include <QDir>

CallWindow::CallWindow(QWidget *parent):
  QMainWindow(parent),
  ui_(new Ui::CallWindow),
  viewFactory_(std::shared_ptr<VideoviewFactory>(new VideoviewFactory)),
  settingsView_(this),
  statsWindow_(nullptr),
  conference_(this),
  partInt_(nullptr),
  timer_(new QTimer(this))
{
  ui_->setupUi(this);
}

CallWindow::~CallWindow()
{
  timer_->stop();
  delete timer_;
  if(statsWindow_)
  {
    statsWindow_->close();
    delete statsWindow_;
  }
  delete ui_;
}

void CallWindow::init(ParticipantInterface *partInt)
{ 
  partInt_ = partInt;

  ui_->Add_contact_widget->setVisible(false);

  viewFactory_->setSelfview(ui_->SelfView, ui_->SelfView);

  setWindowTitle("Kvazzup");

  contacts_.initializeList(ui_->contactList, partInt_);

  aboutWidget_ = new Ui::AboutWidget;
  aboutWidget_->setupUi(&about_);

  // I don't know why this is required.
  qRegisterMetaType<QVector<int> >("QVector<int>");


  QObject::connect(&settingsView_, SIGNAL(settingsChanged()),
                   this, SIGNAL(settingsChanged()));

  QObject::connect(ui_->mic, SIGNAL(clicked()),
                   this, SIGNAL(micStateSwitch()));

  QObject::connect(ui_->camera, SIGNAL(clicked()),
                   this, SIGNAL(cameraStateSwitch()));

  QObject::connect(ui_->EndCallButton, SIGNAL(clicked()),
                   this, SIGNAL(endCall()));

  QObject::connect(ui_->actionClose, SIGNAL(triggered()),
                   this, SIGNAL(closed()));

  QMainWindow::show();

  QObject::connect(&conference_, &ConferenceView::acceptCall, this, &CallWindow::callAccepted);
  QObject::connect(&conference_, &ConferenceView::rejectCall, this, &CallWindow::callRejected);
  QObject::connect(&conference_, &ConferenceView::cancelCall, this, &CallWindow::callCancelled);

  conference_.init(ui_->participantLayout, ui_->participants);

  initButton(QDir::currentPath() + "/icons/add_contact.svg", QSize(60,60), QSize(35,35), ui_->addContact);
  initButton(QDir::currentPath() + "/icons/settings.svg", QSize(60,60), QSize(35,35), ui_->settings_button);
  initButton(QDir::currentPath() + "/icons/photo-camera.svg", QSize(60,60), QSize(35,35), ui_->camera);
  initButton(QDir::currentPath() + "/icons/microphone.svg", QSize(60,60), QSize(35,35), ui_->mic);
  initButton(QDir::currentPath() + "/icons/end_call.svg", QSize(60,60), QSize(35,35), ui_->EndCallButton);

  ui_->buttonContainer->layout()->setAlignment(ui_->endcallHolder, Qt::AlignBottom);
  ui_->buttonContainer->layout()->setAlignment(ui_->settings_button, Qt::AlignBottom);
  ui_->buttonContainer->layout()->setAlignment(ui_->mic, Qt::AlignBottom);
  ui_->buttonContainer->layout()->setAlignment(ui_->camera, Qt::AlignBottom);
  ui_->buttonContainer->layout()->setAlignment(ui_->SelfView, Qt::AlignBottom);

  ui_->contactListContainer->layout()->setAlignment(ui_->addContact, Qt::AlignHCenter);

  ui_->EndCallButton->hide();

  settingsView_.init();
}

void CallWindow::initButton(QString iconPath, QSize size, QSize iconSize, QPushButton* button)
{
  QPixmap pixmap(iconPath);
  if(!pixmap.isNull())
  {
    QIcon ButtonIcon(pixmap);
    button->setIcon(ButtonIcon);
    button->setText("");

    button->setMaximumSize(size);
    button->setMinimumSize(size);
    button->setIconSize(iconSize);
  }
  else
  {
    qDebug() << "WARNING," << metaObject()->className() << ": Could not find icon:" << iconPath;
  }
}

StatisticsInterface* CallWindow::createStatsWindow()
{
  qDebug() << "Initiating, CallWindow: Creating statistics window";
  statsWindow_ = new StatisticsWindow(this);

  // Stats GUI updates are handled solely by timer
  timer_->setInterval(200);
  timer_->setSingleShot(false);
  timer_->start();

  connect(timer_, SIGNAL(timeout()), statsWindow_, SLOT(update()));
  return statsWindow_;
}

void CallWindow::addContact()
{
  contacts_.addContact(partInt_, ui_->peerName->text(), "anonymous", ui_->ip->text());
}

void CallWindow::displayOutgoingCall(uint32_t sessionID, QString name)
{
  conference_.callingTo(sessionID, name); // TODO get name from contact list
}

void CallWindow::displayIncomingCall(uint32_t sessionID, QString caller)
{
  conference_.incomingCall(sessionID, caller);
}

void CallWindow::displayRinging(uint32_t sessionID)
{
  conference_.ringing(sessionID);
}

void CallWindow::openStatistics()
{
  if(statsWindow_)
  {
    statsWindow_->show();
  }
  else
  {
    qDebug() << "WARNING," << metaObject()->className()
             << ": No stats window class initiated";
  }
}

void CallWindow::closeEvent(QCloseEvent *event)
{
  emit closed();
  statsWindow_->hide();
  statsWindow_->finished(0);

  QMainWindow::closeEvent(event);
}

void CallWindow::addVideoStream(uint32_t sessionID)
{
  ui_->EndCallButton->setEnabled(true);
  ui_->EndCallButton->show();
  conference_.addVideoStream(sessionID, viewFactory_);
}

void CallWindow::setMicState(bool on)
{
  if(on)
  {
    initButton(QDir::currentPath() + "/icons/microphone.svg", QSize(60,60), QSize(35,35), ui_->mic);
    //ui_->mic->setText("Mic off");
  }
  else
  {
    initButton(QDir::currentPath() + "/icons/no_microphone.svg", QSize(60,60), QSize(35,35), ui_->mic);
    //ui_->mic->setText("Mic on");
  }
}

void CallWindow::setCameraState(bool on)
{
  if(on)
  {
    initButton(QDir::currentPath() + "/icons/photo-camera.svg", QSize(60,60), QSize(35,35), ui_->camera);
    //ui_->camera->setText("Camera off");
  }
  else
  {
    initButton(QDir::currentPath() + "/icons/no_photo-camera.svg", QSize(60,60), QSize(35,35), ui_->camera);
    //ui_->camera->setText("Camera on");
  }
}

void CallWindow::removeParticipant(uint32_t sessionID)
{
  if(!conference_.removeCaller(sessionID))
  {
    ui_->EndCallButton->setEnabled(false);
    ui_->EndCallButton->hide();
  }
}

void CallWindow::on_settings_button_clicked()
{
  settingsView_.show();
}

void CallWindow::on_about_clicked()
{
  about_.show();
}

void CallWindow::clearConferenceView()
{
  ui_->EndCallButton->setEnabled(false);
  ui_->EndCallButton->hide();
  conference_.close();
}
