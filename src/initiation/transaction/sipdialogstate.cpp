#include "sipdialogstate.h"

#include "initiation/siptransactionuser.h"

#include "common.h"

#include <QDateTime>
#include <QDebug>
#include <QSettings>


SIPDialogState::SIPDialogState():
  localTag_(""),
  remoteTag_(""),
  callID_(""),
  // cseq start value. For example 31-bits of 32-bit clock
  localCSeq_(QDateTime::currentSecsSinceEpoch()%2147483647),
  remoteCSeq_(0),
  sessionState_(false)
{}


void SIPDialogState::createNewDialog(SIP_URI remoteURI, QString localAddress,
                                     uint16_t localPort, bool registered)
{
  printDebug(DEBUG_NORMAL, "SIPDialogState", DC_START_CALL, "Creating a new dialog.");
  initDialog(localAddress, localPort);

  remoteURI_ = remoteURI;
  requestUri_ = remoteURI;
  if(!registered)
  {
    printDebug(DEBUG_NORMAL, "SIPDialogState", DC_START_CALL,
               "Setting peer-to-peer address.");
    localURI_.host = localAddress;
  }
}


void SIPDialogState::createServerConnection(SIP_URI requestURI,
                                            QString ourContactAddress,
                                            uint16_t contactPort)
{
  printDebug(DEBUG_NORMAL, "SIPDialogState", DC_START_CALL,
             "Creating a SIP Server dialog.");
  initDialog(ourContactAddress, contactPort);

  remoteURI_ = localURI_;
  requestUri_ = requestURI; // server has different request uri from remote
  localCSeq_ = 0; //
}


void SIPDialogState::createDialogFromINVITE(std::shared_ptr<SIPMessageInfo> &inMessage,
                                            QString hostName)
{
  printDebug(DEBUG_NORMAL, "SIPDialogState", DC_START_CALL,
             "Creating a dialog from incoming INVITE.");
  Q_ASSERT(callID_ == "");
  Q_ASSERT(inMessage);
  Q_ASSERT(inMessage->dialog);

  if(callID_ != "")
  {
    if(correctRequestDialog(inMessage->dialog, SIP_INVITE, inMessage->cSeq))
    {
      printDebug(DEBUG_PROGRAM_ERROR, "SIPDialogState", DC_START_CALL,
                 "Re-INVITE should be processed differently.");
      return;
    }
    else
    {
      printDebug(DEBUG_PEER_ERROR, "SIPDialogState", DC_START_CALL,
                 "Got a request not belonging to this dialog.");
      return;
    }
  }

  initDialog(hostName, 0, inMessage->dialog->callID); // TODO: port

  remoteURI_ = inMessage->from;
  requestUri_ = inMessage->from;

  localURI_.host = hostName;

  remoteTag_ = inMessage->dialog->fromTag;
  if(remoteTag_ == "")
  {
    printDebug(DEBUG_PEER_ERROR, "SIPDialogState", DC_START_CALL,
               "They did not provide their tag in INVITE!");
    // TODO: send an error response.
  }

  remoteCSeq_ = inMessage->cSeq;

   // set the request to tag to local tag value so when sending the response it is already there.
  if(inMessage->dialog->toTag == "")
  {
    inMessage->dialog->toTag = localTag_;
  }

  qDebug() << "Received a dialog creating INVITE. Creating dialog."
           << "CallID: " << callID_ << "OurTag:" << localTag_ << "Cseq:" << localCSeq_;
}


void SIPDialogState::getRequestDialogInfo(SIPRequest &outRequest, QString localAddress)
{
  Q_ASSERT(localURI_.username != "" && localURI_.host != "");
  Q_ASSERT(remoteURI_.username != "" && remoteURI_.host != "");

  if(localURI_.username == "" || localURI_.host == "" ||
     remoteURI_.username == "" || remoteURI_.host == "")
  {
    printDebug(DEBUG_PROGRAM_ERROR, "SIPDialogState", DC_SEND_SIP_REQUEST,
               "The dialog state info has not been set, but we are using it.",
                {"username", "host", "remote username", "remote host"},
                {localURI_.username, localURI_.host, remoteURI_.username, remoteURI_.host});
  }

  outRequest.requestURI = requestUri_;

  if(outRequest.type != SIP_ACK && outRequest.type != SIP_CANCEL)
  {
    ++localCSeq_;
    printDebug(DEBUG_NORMAL, "SIPDialogState", DC_SEND_SIP_REQUEST, "Increasing CSeq",
              {"CSeq"}, {QString::number(localCSeq_)});
  }

  outRequest.message->cSeq = localCSeq_;
  outRequest.message->from = localURI_;
  outRequest.message->to = remoteURI_;
  outRequest.message->contact = localContactUri_;
  outRequest.message->vias.push_back(getLocalVia(localAddress, 0)); // TODO: port

  // SIPDialogInfo format: toTag, fromTag, CallID
  outRequest.message->dialog
      = std::shared_ptr<SIPDialogInfo> (new SIPDialogInfo{remoteTag_, localTag_, callID_});
}


ViaInfo SIPDialogState::getLocalVia(QString localAddress, uint16_t port)
{
  return ViaInfo{TCP, "2.0", localAddress,
        QString("z9hG4bK" + generateRandomString(BRANCHLENGTH)), port};
}


bool SIPDialogState::correctRequestDialog(std::shared_ptr<SIPDialogInfo> dialog,
                                          RequestType type, uint32_t remoteCSeq)
{
  Q_ASSERT(callID_ != "");
  if(callID_ == "")
  {
    qDebug() << "WARNING: The SIP dialog has not been initialized, but it is used";
    return false;
  }

  // TODO: For backwards compability, this should be prepared for missing To-tag (or was it from tag) (RFC3261).
  // if our tags and call-ID match the incoming requests, it belongs to this dialog
  if((dialog->toTag == localTag_) && dialog->fromTag == remoteTag_ &&
     ( dialog->callID == callID_))
  {
    // The request cseq should be larger than our remotecseq.
    if(remoteCSeq <= remoteCSeq_ && type != SIP_ACK && type != SIP_CANCEL)
    {
      qDebug() << "PEER_ERROR:"
               << "Their request Cseq was smaller than their previous cseq which is not permitted!";
      // TODO: if remote cseq in message is lower than remote cseq, send 500
      return false;
    }

    remoteCSeq_ = remoteCSeq;
    return true;
  }
  return false;
}


bool SIPDialogState::correctResponseDialog(std::shared_ptr<SIPDialogInfo> dialog, uint32_t messageCSeq)
{
  // For backwards compability, this should be prepared for missing To-tag (or was it from tag) (RFC3261).
  // if our tags and call-ID match the incoming requests, it belongs to this dialog
  if((dialog->fromTag == localTag_) && dialog->toTag == remoteTag_ || remoteTag_ == "" &&
     ( dialog->callID == callID_))
  {
    // The response cseq should be the same as our cseq
    if(messageCSeq != localCSeq_)
    {
      qDebug() << "PEER_ERROR:" << "The response CSeq was not the same as our previous request!"
               << messageCSeq << "vs local" << localCSeq_;
      // TODO: if remote cseq in message is lower than remote cseq, send 500
      return false;
    }

    if(remoteTag_ == "")
    {
      qDebug() << "We don't yet have their remote Tag. Using the one in response.";
      remoteTag_ = dialog->toTag;
    }

    return true;
  }
  return false;
}


void SIPDialogState::initDialog(QString localAddress, uint16_t localPort)
{
  initLocalURI();
  initContactURI(localAddress, localPort);
  initCallInfo();
}


void SIPDialogState::initDialog(QString localAddress, uint16_t localPort, QString callID)
{
  initLocalURI();
  initContactURI(localAddress, localPort);
  callID_ = callID;

  if(localTag_ == "")
  {
    localTag_ = generateRandomString(TAGLENGTH);
  }
}


void SIPDialogState::initLocalURI()
{
  // init stuff from the settings
  QSettings settings("kvazzup.ini", QSettings::IniFormat);

  localURI_.realname = settings.value("local/Name").toString();
  localURI_.username = settings.value("local/Username").toString();
  localURI_.host = settings.value("sip/ServerAddress").toString();
  localURI_.connection = TCP; // TODO: Implement TLS
  localURI_.port = 0; // we don't add port to our server address

  if(localURI_.username.isEmpty())
  {
    localURI_.username = "anonymous";
  }
}


void SIPDialogState::initContactURI(QString localAddress, uint16_t localPort)
{
  // init stuff from the settings
  QSettings settings("kvazzup.ini", QSettings::IniFormat);

  localContactUri_.realname = "";
  localContactUri_.username = settings.value("local/Username").toString();
  localContactUri_.host = localAddress;
  localContactUri_.connection = TCP; // TODO: Implement TLS
  localContactUri_.port = localPort;

  if(localContactUri_.username.isEmpty())
  {
    localContactUri_.username = "anonymous";
  }
}


void SIPDialogState::initCallInfo()
{
  localTag_ = generateRandomString(TAGLENGTH);
  callID_ = generateRandomString(CALLIDLENGTH);
  if(localURI_.host != "")
  {
    callID_ += "@" + localURI_.host;
  }

  qDebug() << "Local dialog created. CallID: " << callID_
           << "Tag:" << localTag_ << "Cseq:" << localCSeq_;
}

