#include "sipservertransaction.h"

#include "initiation/siptransactionuser.h"
#include "initiation/transaction/sipdialogstate.h"

#include "common.h"

#include <QDebug>

SIPServerTransaction::SIPServerTransaction():
  sessionID_(0),
  receivedRequest_(nullptr),
  transactionUser_(nullptr)
{}

void SIPServerTransaction::init(SIPTransactionUser* tu, uint32_t sessionID)
{
  transactionUser_ = tu;
  sessionID_ = sessionID;
}

/*
void SIPServerTransaction::setCurrentRequest(SIPRequest& request)
{
  copyMessageDetails(request.message, receivedRequest_);
}
*/

// processes incoming request
bool SIPServerTransaction::processRequest(SIPRequest& request,
                                          std::shared_ptr<SIPDialogState> state)
{
  Q_ASSERT(transactionUser_ && sessionID_);
  if(!transactionUser_ || sessionID_ == 0)
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_RECEIVE_SIP_RESPONSE,
               "SIP Server transaction not initialized.");
    return false;
  }

  // TODO: check that the request is appropriate at this time.

  if((receivedRequest_ == nullptr && request.type != SIP_ACK) || request.type == SIP_BYE)
  {
    copyMessageDetails(request.message, receivedRequest_);
  }
  else if (request.type != SIP_ACK)
  {
    printDebug(DEBUG_PEER_ERROR, "SIP Server Transaction", DC_RECEIVE_SIP_REQUEST,
               "They sent us a new SIP request even though we have the old one still saved.",
                {"SessionID"}, {QString::number(sessionID_)});
    return false;
  }

  switch(request.type)
  {
  case SIP_INVITE:
  {
    if (!state->getState())
    {
      if (!transactionUser_->incomingCall(sessionID_, request.message->from.realname))
      {
        // if we did not auto-accept
        responseSender(SIP_RINGING);
      }
    }
    else {
      responseSender(SIP_OK);
    }
    break;
  }
  case SIP_ACK:
  {
    state->setState(true);
    transactionUser_->callNegotiated(sessionID_);
    break;
  }
  case SIP_BYE:
  {
    state->setState(false);
    transactionUser_->endCall(sessionID_);
    return false;
  }
  case SIP_CANCEL:
  {
    transactionUser_->cancelIncomingCall(sessionID_);
    return false;
  }
  case SIP_OPTIONS:
  {
    qDebug() << "Don't know what to do with OPTIONS yet";
    break;
  }
  case SIP_REGISTER:
  {
    qDebug() << "Why on earth are we receiving REGISTER methods?";
    responseSender(SIP_NOT_ALLOWED);
    break;
  }
  default:
  {
    qDebug() << "Unsupported request type received";
    responseSender(SIP_NOT_ALLOWED);
    break;
  }
  }
  return true;
}

void SIPServerTransaction::getResponseMessage(std::shared_ptr<SIPMessageInfo> &outMessage,
                                              ResponseType type)
{
  if(receivedRequest_ == nullptr)
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_SEND_SIP_RESPONSE,
               "The received request was not set before trying to use it!");
    return;
  }
  copyMessageDetails(receivedRequest_, outMessage);
  outMessage->maxForwards = 71;
  outMessage->version = "2.0";
  outMessage->contact = SIP_URI{"","","", TCP, 0}; // TODO: No contact for reply?
  outMessage->content.length = 0;
  outMessage->content.type = NO_CONTENT;

  int responseCode = type;
  if(responseCode >= 200)
  {
    printDebug(DEBUG_NORMAL, this, DC_SEND_SIP_RESPONSE,
               "Sending a final response. Deleting request details.",
               {"SessionID", "Code", "Cseq"},
               {QString::number(sessionID_), QString::number(responseCode),
                QString::number(receivedRequest_->cSeq)});
    receivedRequest_.reset();
    receivedRequest_ = nullptr;
  }
}

void SIPServerTransaction::acceptCall()
{
  responseSender(SIP_OK);
}

void SIPServerTransaction::rejectCall()
{
  responseSender(SIP_DECLINE);
}

void SIPServerTransaction::responseSender(ResponseType type)
{
  Q_ASSERT(receivedRequest_ != nullptr);
  emit sendResponse(sessionID_, type);
}

void SIPServerTransaction::copyMessageDetails(std::shared_ptr<SIPMessageInfo>& inMessage,
                        std::shared_ptr<SIPMessageInfo>& copy)
{
  Q_ASSERT(inMessage);
  copy = std::shared_ptr<SIPMessageInfo> (new SIPMessageInfo());
  copy->dialog = std::shared_ptr<SIPDialogInfo> (new SIPDialogInfo());
  // Which fields to copy are listed in section 8.2.6.2 of RFC 3621

  // from-field
  copy->from = inMessage->from;
  copy->dialog->fromTag = inMessage->dialog->fromTag;

  // Call-ID field
  copy->dialog->callID = inMessage->dialog->callID;

  // CSeq
  copy->cSeq = inMessage->cSeq;
  copy->transactionRequest = inMessage->transactionRequest;

  // Via- fields in same order
  for(ViaInfo via : inMessage->senderReplyAddress)
  {
    copy->senderReplyAddress.push_back(via);
  }

  // To field, expect if To tag is missing, in which case it should be added
  // To tag is added in dialog when checking the first request.
  Q_ASSERT(inMessage->dialog->toTag != "");
  copy->to = inMessage->to;
  copy->dialog->toTag = inMessage->dialog->toTag;
}
