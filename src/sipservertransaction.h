#pragma once

#include "siptypes.h"


class SIPTransactionUser;

class SIPServerTransaction : public QObject
{
   Q_OBJECT
public:
  SIPServerTransaction();

  void init(SIPTransactionUser* tu, uint32_t sessionID);

  // processes incoming request. Part of our server transaction
  void processRequest(SIPRequest& request);
  void wrongRequestDestination();
  void malformedRequest();

  void acceptCall();
  void rejectCall();

signals:

  void sendResponse(uint32_t sessionID, ResponseType type, RequestType originalRequest);

private:

  void responseSender(ResponseType type, bool finalResponse);
  bool goodRequest(); // use this to filter out untimely/duplicate requests

  uint32_t sessionID_;

  RequestType waitingResponse_;

  SIPTransactionUser* transactionUser_;
};
