#pragma once

#include "siptypes.h"

#include <QTimer>
#include <QObject>

class SIPTransactionUser;

enum CallState {INACTIVE, NEGOTIATING, RUNNNING, ENDING};

class SIPClientTransaction : public QObject
{
  Q_OBJECT
public:
  SIPClientTransaction();

  void init(SIPTransactionUser* tu, uint32_t sessionID);
  bool ourResponse(SIPResponse& response)
  {
    return false;
  }

  void getRequestMessageInfo(RequestType type,
                             std::shared_ptr<SIPMessageInfo> &outMessage);

  //processes incoming response. Part of our client transaction
  void processResponse(SIPResponse& response);
  void wrongResponseDestination();
  void malformedResponse();
  void responseIsError();

  bool startCall();
  void endCall();
  void registerToServer();

  void connectionReady(bool ready);

signals:
  // send messages to other end
  void sendRequest(uint32_t sessionID, RequestType type);

private slots:
  void requestTimeOut();

private:
  void requestSender(RequestType type);
  bool goodResponse(); // use this to filter out untimely/duplicate responses

  // used to determine what type of request the response is for
  RequestType ongoingTransactionType_;

  // TODO: Probably need to record the whole message and check that the details are ok.

  QTimer requestTimer_;
  bool connected_;

  uint32_t sessionID_;

  CallState state_;

  // waiting to be sent once the connecion has been opened
  RequestType pendingRequest_;

  SIPTransactionUser* transactionUser_;
};
