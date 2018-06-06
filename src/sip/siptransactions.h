#pragma once

#include "globalsdpstate.h"
#include "sip/siptransport.h"
#include "sip/sipregistration.h"
#include "connectionserver.h"

#include "common.h"

/* This class manages the interactions between different components in
 * a SIP transaction. This class should implement as little functionality
 * as possible and only focus on interractions.
 */

/* Some terms from RFC 3621:
 * Dialog = a SIP dialog constructed with INVITE-transaction
 * Session = a media session negotiated in INVITE-transaction
 */


// TODO: some kind of address book class might be useful because the connection addresses can change.

struct Contact
{
  QString username;
  QString realName;
  QString remoteAddress;
  bool proxyConnection;
};

class SIPRouting;
class SIPDialogState;
class SIPTransactionUser;
class SIPServerTransaction;
class SIPClientTransaction;

class SIPTransactions : public QObject
{
  Q_OBJECT
public:
  SIPTransactions();

  void init(SIPTransactionUser* callControl);
  void uninit();

  void setServerAddress(QString server);

  QList<uint32_t> startCall(QList<Contact> addresses);
  void acceptCall(uint32_t sessionID);
  void rejectCall(uint32_t sessionID);
  void endCall(uint32_t sessionID);

  void endAllCalls();

  void makeConference();
  void dispandConference();

  void getSDPs(uint32_t sessionID,
               std::shared_ptr<SDPMessageInfo>& localSDP,
               std::shared_ptr<SDPMessageInfo>& remoteSDP);

private slots:
  // connection has been established. This enables for us to get the needed info
  // to form a SIP message
  void connectionEstablished(quint32 transportID, QString localAddress, QString remoteAddress);
  void receiveTCPConnection(TCPConnection* con);

  void processSIPRequest(SIPRequest request, quint32 transportID, QVariant& content);
  void processSIPResponse(SIPResponse response, quint32 transportID, QVariant& content);

  void sendRequest(uint32_t sessionID, RequestType type);
  void sendResponse(uint32_t sessionID, ResponseType type, RequestType originalRequest);

private:

  // TODO: Transports should be separate from dialogs

  struct SIPDialogData
  {
    std::shared_ptr<SIPDialogState> state;
    // do not stop connection before responding to all requests
    std::shared_ptr<SIPServerTransaction> server;
    std::shared_ptr<SIPClientTransaction> client;
    std::shared_ptr<SDPMessageInfo> localSdp_;
    std::shared_ptr<SDPMessageInfo> remoteSdp_;

    bool proxyConnection_;
    quint32 transportID;
  };

  std::shared_ptr<SIPDialogState> createSIPDialogState(uint32_t sessionID);
  std::shared_ptr<SIPTransport> createSIPTransport();
  std::shared_ptr<SIPRouting> createSIPRouting(QString remoteUsername,
                                               QString localAddress,
                                               QString remoteAddress, bool hostedSession);

  // returns whether we should continue with processing
  bool processSDP(uint32_t sessionID, QVariant &content, QHostAddress localAddress);

  void createLocalDialog(QString remoteUsername, QString remoteAddress);
  void createRemoteDialog(TCPConnection* con);
  void createDialog(std::shared_ptr<SIPDialogData>& dialog);
  void destroyDialog(uint32_t sessionID);

  // This mutex makes sure that the dialog has been added to the dialogs_ list
  // before we are accessing it when receiving messages
  QMutex connectionMutex_;

  // sessionID:s are positions in this list. SessionID:s are used in this program to
  // keep track of dialogs. The CallID is not used because we could be calling ourselves
  // and using uint32_t is simpler than keeping track of tags

  // TODO: separate dialog forming from dialog

  QList<std::shared_ptr<SIPDialogData>> dialogs_;

  QList<std::shared_ptr<SIPTransport>> transports_;
  bool isConference_;

  SIPRegistration registration_;

  // used with non-dialog messages
  std::shared_ptr<SIPClientTransaction> generalClient;
  std::shared_ptr<SIPServerTransaction> generalServer;

  GlobalSDPState sdp_;

  ConnectionServer server_;
  uint16_t sipPort_;

  // use this client to register us to a server
  std::shared_ptr<SIPClientTransaction> registerClient_;

  SIPTransactionUser* transactionUser_;
};
