#pragma once

#include "sipclient.h"

// Sending SIP Requests and processing of SIP responses that don't belong to
// a dialog. Typical example is the REGISTER-method. OPTIONS can also sent
// without a dialog.


class SIPNonDialogClient : public SIPClient
{
  Q_OBJECT
public:
  SIPNonDialogClient();
  ~SIPNonDialogClient();

  void set_remoteURI(SIP_URI& uri);

  // constructs the SIP message info struct as much as possible
  virtual void getRequestMessageInfo(RequestType type,
                             std::shared_ptr<SIPMessageInfo> &outMessage);

  virtual bool processResponse(SIPResponse& response,
                               SIPDialogState& state);

  void registerToServer();
  void unRegister();

signals:
  void sendNondialogRequest(SIP_URI& uri, RequestType type);

private:

  SIP_URI remoteUri_;

  unsigned int expires_;
};
