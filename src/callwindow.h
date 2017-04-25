#pragma once

#include <QMainWindow>

#include "MediaManager.h"
#include "callnegotiation.h"

class StatisticsWindow;

namespace Ui {
class CallWindow;
class CallerWidget;
}

class CallWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit CallWindow(QWidget *parent, uint16_t width, uint16_t height, QString name);
  ~CallWindow();

  void startStream();

  void closeEvent(QCloseEvent *event);

public slots:
   void addParticipant();
   void openStatistics(); // Opens statistics window

   void micState();
   void cameraState();

   void incomingCall(QString callID, QString caller);
   void callOurselves(std::shared_ptr<SDPMessageInfo> info);

   void ringing(QString callID);

   void ourCallRejected(QString callID);

   void callNegotiated(QString CallID, std::shared_ptr<SDPMessageInfo> peerInfo,
                       std::shared_ptr<SDPMessageInfo> localInfo);

   // UI messages
   void acceptCall();
   void rejectCall();

   void endCall(QString callID);

private:

  void createParticipant(const std::shared_ptr<SDPMessageInfo> peerInfo,
                         const std::shared_ptr<SDPMessageInfo> localInfo);
  void hideLabel();

  void processNextWaitingCall();

  Ui::CallWindow *ui_;
  Ui::CallerWidget *ui_widget_;

  StatisticsWindow *stats_;
  QWidget* callingWidget_;

  CallNegotiation callNeg_;
  MediaManager media_;

  QTimer *timer_; // for GUI update

  // dynamic videowidget adding to layout
  int row_;
  int column_;

  QSize currentResolution_;

  uint16_t portsOpen_;

  QString name_;

  struct CallInfo
  {
    QString callID;
    QString name;
  };

  QMutex callWaitingMutex_;
  QList<CallInfo> waitingCalls_;
  std::vector<CallInfo> currentCalls_;

  bool ringing_;
};
