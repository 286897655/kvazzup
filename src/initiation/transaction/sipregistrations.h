#pragma once

#include "initiation/transaction/sipnondialogclient.h"
#include "initiation/transaction/sipdialogstate.h"

#include "initiation/siptypes.h"

#include <QObject>
#include <QTimer>


class SIPNonDialogClient;
class SIPDialogState;
class SIPTransactionUser;
class ServerStatusView;

class SIPRegistrations : public QObject
{
  Q_OBJECT
public:
  SIPRegistrations();

  void init(ServerStatusView* statusView);

  // return if we can delete
  void uninit();

  void bindToServer(QString serverAddress, QString localAddress,
                    uint16_t port);

  // Identify if this reponse is to our REGISTER-request
  bool identifyRegistration(SIPResponse& response, QString &outAddress);

  void processNonDialogResponse(SIPResponse& response);

  void sendNonDialogRequest(SIP_URI& uri, RequestType type);

  bool haveWeRegistered();


signals:

  void transportProxyRequest(QString& address, SIPRequest& request);

private slots:

  void refreshRegistrations();

private:

  struct SIPRegistrationData
  {
    SIPNonDialogClient client;
    SIPDialogState state;

    QString contactAddress;
    uint16_t contactPort;

    bool active;
    bool updatedContact;
  };

  std::map<QString, std::shared_ptr<SIPRegistrationData>> registrations_;

  ServerStatusView* statusView_;
  QTimer retryTimer_;
};
