#pragma once

#include "icetypes.h"

#include <QWaitCondition>
#include <QMutex>
#include <QThread>
#include <QList>
#include <QMap>

#include <memory>



class IceSessionTester : public QThread
{
  Q_OBJECT

public:
  IceSessionTester(bool controller, int timeout);
  ~IceSessionTester();

  void init(QList<std::shared_ptr<ICEPair>> *candidates,
            uint32_t sessionID);

signals:
  // When FlowAgent finishes, it sends a ready signal to main thread (ICE).
  // If the nomination succeeded, both candidateRTP and candidateRTCP are valid pointers,
  // otherwise nullptr.
  void ready(QList<std::shared_ptr<ICEPair>>& streams,
             uint32_t sessionID);

  // when both RTP and RTCP of any address succeeds, sends endNomination() signal to FlowAgent (sent by ConnectionTester)
  // so it knows to stop all other ConnectionTester and return the succeeded pair to ICE
  //
  // FlowAgent listens to this signal in waitForEndOfNomination() and if the signal is not received within some time period,
  // FlowAgent fails and returns nullptrs to ICE indicating that the call cannot start
  void endNomination();

public slots:
  void nominationDone(std::shared_ptr<ICEPair> connection);

protected:
  virtual void run();

private:

  // wait for endNomination() signal and return true if it's received (meaning the nomination succeeded)
  // if the endNomination() is not received in time the function returns false
  bool waitForEndOfNomination(unsigned long timeout);

  QList<std::shared_ptr<ICEPair>> *candidates_;

  uint32_t sessionID_;

  bool controller_;
  int timeout_;

  // temporary storage for succeeded pairs, when both RTP and RTCP
  // of some candidate pair succeeds, endNomination() signal is emitted
  // and the succeeded pair is copied to nominated_rtp_ and nominated_rtcp_
  //
  // the first candidate pair that has both RTP and RTCP tested is chosen
  QMap<QString, std::pair<std::shared_ptr<ICEPair>, std::shared_ptr<ICEPair>>> nominated_;

  std::shared_ptr<ICEPair> nominated_rtp_;
  std::shared_ptr<ICEPair> nominated_rtcp_;
  QMutex nominated_mtx;
};
