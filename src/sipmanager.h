#pragma once

#include "SIPState.h"
#include "globalsdpstate.h"
#include "connection.h"
#include "connectionserver.h"

#include "common.h"

struct Contact
{
  QString username;
  QString realName;
  QString remoteAddress;
};

class SIPRouting;

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

private slots:
  // connection has been established. This enables for us to get the needed info
  // to form a SIP message
  void connectionEstablished(quint32 sessionID);
  void receiveTCPConnection(Connection* con);
  void processSIPMessage(QString header, QString content, quint32 sessionID);

private:

  struct SIPSession
  {
    QString callID;
    Connection* con;
    SIPState* state;
    SIPRouting* routing;
    // has local invite sdp or o response sdp
    std::shared_ptr<SDPMessageInfo> localSdp_;
    // empty until final ok 200
    std::shared_ptr<SDPMessageInfo> remoteSdp_;
    bool hostedSession;
  };

  SIPState* createSIPState();
  void sendRequest(RequestType request, SIPSession *session);

  void destroySession(SIPSession *session);

  QMutex sessionMutex_;

  QList<SIPSession*> sessions_;

  GlobalSDPState sdp_;
  SIPStringComposer messageComposer_;

  ConnectionServer server_;
  uint16_t sipPort_;

  QString localName_;
  QString localUsername_;
};
