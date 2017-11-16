#pragma once

#include "SIPState.h"
#include "connection.h"
#include "connectionserver.h"

class SIPManager : public QObject
{
  Q_OBJECT
public:
  SIPManager();

  void init();
  void uninit();

  QList<QString> startCall(QList<Contact> addresses);
  void acceptCall(QString callID);
  void rejectCall(QString callID);
  void endCall(QString callID);

  void endAllCalls();

signals:

  // caller wants to establish a call.
  // Ask use if it is and call accept or reject call
  void incomingINVITE(QString CallID, QString caller);

  // we are calling ourselves.
  // TODO: Current implementation ceases the negotiation and just starts the call.
  void callingOurselves(QString callID, std::shared_ptr<SDPMessageInfo> info);

  // their call which we have accepted has finished negotiating. Call can now start.
  void callNegotiated(QString callID, std::shared_ptr<SDPMessageInfo> peerInfo,
                      std::shared_ptr<SDPMessageInfo> localInfo);

  // Local call is waiting for user input at remote end
  void ringing(QString callID);

  // Call iniated locally has been accepted by peer. Call can now start.
  void ourCallAccepted(QString callID, std::shared_ptr<SDPMessageInfo> peerInfo,
                       std::shared_ptr<SDPMessageInfo> localInfo);

  // Remote rejected local INVITE
  void ourCallRejected(QString CallID);

  // Received call ending signal (BYE)
  void callEnded(QString callID, QString ip);

private:

  struct SIPSession
  {
    QString callID;
    Connection *con;
    SIPState *state;
  };

  SIPState state_;

  QMutex sessionMutex_;
  //callID to session
  QList<SIPSession*> sessions_;

  SIPStringComposer messageComposer_;

  ConnectionServer server_;
};
