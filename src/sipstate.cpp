#include "sipstate.h"

#include "connection.h"
#include "sipparser.h"

#include <QSettings>

//TODO use cryptographically secure callID generation to avoid collisions.
const QString alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                         "abcdefghijklmnopqrstuvwxyz"
                         "0123456789";

const uint16_t CALLIDLENGTH = 16;
const uint16_t TAGLENGTH = 16;
const uint16_t BRANCHLENGTH = 16;
const uint16_t MAXFORWARDS = 70; // the recommmended value is 70
const uint16_t STARTPORT = 18888;

SIPState::SIPState():
  session_(),
  messageComposer_(),
  localName_("Anonymous"),
  firstAvailablePort_(STARTPORT)
{}

SIPState::~SIPState()
{}

void SIPState::init()
{
  qsrand(1);
/*
  QObject::connect(&server_, SIGNAL(newConnection(Connection*)),
                   this, SLOT(receiveConnection(Connection*)));

  // listen to everything
  // TODO: maybe record the incoming connection address and choose used network interface address
  // according to that
  server_.listen(QHostAddress("0.0.0.0"), sipPort_);
*/
  QSettings settings;
  QString localName = settings.value("local/Name").toString();
  QString localUsername = settings.value("local/Username").toString();

  if(!localName.isEmpty())
  {
    localName_ = localName;
  }
  else
  {
    localName_ = "Anonymous";
  }

  if(!localUsername.isEmpty())
  {
    localUsername_ = localUsername;
  }
  else
  {
    localUsername_ = "anonymous";
  }
}

void SIPState::uninit()
{
  for (uint16_t connectionID = 1; connectionID <= connections_.size();
       ++connectionID)
  {
    stopConnection(connectionID);
  }
  connections_.empty();
}

std::shared_ptr<SDPMessageInfo> SIPState::generateSDP(QString localAddress)
{
  // TODO: Get suitable SDP from media manager
  QString sdp_str = "v=0 \r\n"
                   "o=" + localUsername_ + " 1234 12345 IN IP4 " + localAddress + "\r\n"
                   "s=Kvazzup\r\n"
                   "t=0 0\r\n"
                   "c=IN IP4 " + localAddress + "\r\n"
                   "m=video " + QString::number(firstAvailablePort_) + " RTP/AVP 97\r\n"
                   "m=audio " + QString::number(firstAvailablePort_ + 2) + " RTP/AVP 96\r\n";

  firstAvailablePort_ += 4; // video and audio + rtcp for both

  std::shared_ptr<SDPMessageInfo> sdp = parseSDPMessage(sdp_str);

  if(sdp == NULL)
  {
    qWarning() << "WARNING: Failed to generate SDP info fomr following sdp message:" << sdp_str;
  }
  return sdp;
}

void SIPState::endCall()
{
  sessionMutex_.lock();
  sendRequest(BYE, session_);
  uninitSession(session_);
  sessionMutex_.lock();
}

QString SIPState::startCall(Contact address)
{
  qDebug() << "Starting call negotiation with " << address.name << "at" << address.contactAddress;

  std::shared_ptr<SIPState::SIPSessionInfo> info = newSIPSessionInfo();
  //Connection* con = new Connection(connections_.size() + 1, true);
  connectionMutex_.lock();
  //connections_.push_back(con);
  //info->connectionID = con->getID();
  connectionMutex_.unlock();

  qDebug() << "Creating connection with ID:" << info->connectionID;
/*
  QObject::connect(con, SIGNAL(messageAvailable(QString, QString, quint32)),
                   this, SLOT(processMessage(QString, QString, quint32)));

  QObject::connect(con, SIGNAL(connected(quint32)),
                   this, SLOT(connectionEstablished(quint32)));
*/
  info->contact = address;

  if(info->contact.name.isEmpty())
  {
    qWarning() << "Warning: No name was given for callee";
    info->contact.name = "Unknown";
  }

  if(info->contact.username.isEmpty())
  {
    qWarning() << "Warning: No username was given for callee";
    info->contact.username = "unknown";
  }

  info->remoteTag = "";

  //con->establishConnection(address.contactAddress, sipPort_);
  // message is sent only after connection has been established so we know our address

  return info->callID;
}

void SIPState::acceptCall()
{
  sendResponse(OK_200, session_);
}

void SIPState::rejectCall()
{
  sendResponse(DECLINE_603, session_);
}


void SIPState::setPeerConnection(QString ourAddress, QString theirAddress)
{

}

void SIPState::setServerConnection(QString hostAddress)
{
  qWarning() << "WARNING: Servered connection not implemented yet.";
}

void SIPState::messageComposition(messageID id, std::shared_ptr<SIPSessionInfo> info)
{
  messageComposer_.to(id, info->contact.name, info->contact.username,
                        info->contact.contactAddress, info->remoteTag);
  messageComposer_.fromIP(id, localName_, localUsername_, info->localAddress, info->localTag);
  QString branch = generateRandomString(BRANCHLENGTH);
  messageComposer_.viaIP(id, info->localAddress, branch);
  messageComposer_.maxForwards(id, MAXFORWARDS);
  messageComposer_.setCallID(id, info->callID, info->host);
  messageComposer_.sequenceNum(id, 1, info->originalRequest);

  QString SIPRequest = messageComposer_.composeMessage(id);

  qDebug() << "Sending the following SIP Request:";
  qDebug() << SIPRequest;

  QByteArray message = SIPRequest.toUtf8();
  if(connections_.at(info->connectionID - 1) != 0)
  {
    connections_.at(info->connectionID - 1)->sendPacket(message);
  }
  else
  {
    qWarning() << "WARNING: Tried to send message with destroyed connection!";
  }
}

void SIPState::sendRequest(RequestType request, std::shared_ptr<SIPSessionInfo> info)
{
  info->originalRequest = request;
  messageID id = messageComposer_.startSIPRequest(request);

  if(request == INVITE)
  {
    QString sdp_str = messageComposer_.formSDP(info->localSDP);
    messageComposer_.addSDP(id, sdp_str);
  }

  messageComposition(id, info);
}

void SIPState::sendResponse(ResponseType response, std::shared_ptr<SIPSessionInfo> info)
{
  messageID id = messageComposer_.startSIPResponse(response);

  if(info->originalRequest == INVITE && response == OK_200)
  {
    QString sdp_str = messageComposer_.formSDP(info->localSDP);
    messageComposer_.addSDP(id, sdp_str);
  }

  messageComposition(id, info);
}

void SIPState::processMessage(QString header, QString content,
                                     quint32 connectionID)
{
  qDebug() << "Processing message";

  if(connectionID != 0)
  {
    std::shared_ptr<SIPMessageInfo> info = parseSIPMessage(header);
    if(info == NULL)
    {
      qDebug() << "Failed to parse SIP message";
      //sendResponse(MALFORMED_400, ); TODO: get info and attempted request from somewhere
      return;
    }

    std::shared_ptr<SDPMessageInfo> sdpInfo = NULL;
    if(info->contentType == "application/sdp" && content.size() != 0)
    {
      sdpInfo = parseSDPMessage(content);
      if(sdpInfo == NULL)
      {
        qDebug() << "SDP parsing failed";
        return;
        //sendResponse(MALFORMED_400, );
      }
    }

    qDebug() << "Message parsed";

    if(info != 0)
    {
      Q_ASSERT(info->callID != "");
      sessionMutex_.lock();
      if(compareSIPSessionInfo(info, connectionID))
      {
        newSIPSessionInfoFromMessage(info, connectionID);

        if(info->request != NOREQUEST && info->response == NORESPONSE)
        {
          processRequest(info, sdpInfo, connectionID);
        }
        else if(info->request == NOREQUEST && info->response != NORESPONSE)
        {
          processResponse(info, sdpInfo);
        }
        else
        {
          qWarning() << "WARNING: No response or request indicated in parsing";
        }
      }
      else
      {
        // TODO: send 400
        qDebug() << "Problem detected in SIP message based on previous information!!";
      }
      sessionMutex_.unlock();
    }
    else
    {
      qDebug() << "Invalid SIP message received";
    }
  }
}

QString SIPState::generateRandomString(uint32_t length)
{
  // TODO make this cryptographically secure
  QString string;
  for( unsigned int i = 0; i < length; ++i )
  {
    string.append(alphabet.at(qrand()%alphabet.size()));
  }
  return string;
}

std::shared_ptr<SIPState::SIPSessionInfo> SIPState::newSIPSessionInfo()
{
  std::shared_ptr<SIPSessionInfo> info (new SIPSessionInfo);

  info->callID = generateRandomString(CALLIDLENGTH);

  qDebug() << "Generated CallID: " << info->callID;

  info->localTag = generateRandomString(TAGLENGTH);

  qDebug() << "Generated tag: " << info->localTag;

  sessionMutex_.lock();
  session_ = info;
  sessionMutex_.unlock();
  return info;
}

void SIPState::newSIPSessionInfoFromMessage(std::shared_ptr<SIPMessageInfo> mInfo,
                                            quint32 connectionID)
{
  std::shared_ptr<SIPState::SIPSessionInfo> info;

  if(session_ != NULL)
  {
    info = session_;
  }
  else
  {
    info = std::shared_ptr<SIPState::SIPSessionInfo> (new SIPSessionInfo);
  }

  info->callID = mInfo->callID;
  info->contact = {mInfo->remoteName, mInfo->remoteUsername, mInfo->contactAddress};
  info->replyAddress = mInfo->localLocation;
  info->localAddress = mInfo->localLocation;
  info->host = mInfo->host;

  if(!mInfo->localTag.isEmpty())
  {
    info->localTag = mInfo->localTag;
  }
  else
  {
    info->localTag = generateRandomString(TAGLENGTH);
  }
  info->remoteTag = mInfo->remoteTag;

  //TODO evaluate SDP

  info->connectionID = connectionID;
  info->originalRequest = mInfo->originalRequest;

  if(info->localSDP == NULL)
  {
    info->localSDP = generateSDP(mInfo->localLocation);
  }

  // TODO: make user the sdp is checked somewhere.
  session_ = info;
}

bool SIPState::compareSIPSessionInfo(std::shared_ptr<SIPMessageInfo> mInfo,
                                         quint32 connectionID)
{
  qDebug() << "Checking SIP message by comparing it to existing information.";

  if(connectionID > connections_.size() || connections_.at(connectionID - 1) == 0)
  {
    qWarning() << "WARNING: Bad connection ID:"
               << connectionID << "/" << connections_.size();
    return false;
  }

  // TODO: This relies on outside information.
  // Maybe keep the SIPSessionInfo until our BYE has been received?
  if(mInfo->localLocation == mInfo->remoteLocation)
  {
    qDebug() << "It is us. Lets forget checking the message";
    return true;
  }

  // if we don't have previous information
  if(session_ == NULL)
  {
    qDebug() << "New Call-ID detected:" << mInfo->callID << "Creating session...";

    if(mInfo->localTag != "" && STRICTSIPPROCESSING)
    {
      qDebug() << "WHY DO THEY HAVE OUR TAG BEFORE IT WAS GENERATED!?!";
      return false;
    }

    if(mInfo->response != NORESPONSE)
    {
      qDebug() << "Got a response as a first messsage. Discarding.";
      return false;
    }

    if(mInfo->request == BYE // No call to end
       || mInfo->request == ACK // No INVITE response to confirm
       || mInfo->request == CANCEL // No request to be cancelled
       || mInfo->request == NOREQUEST)
    {
      qDebug() << "Got an unsuitable request as first request:" << mInfo->request;
      return false;
    }

    if(mInfo->cSeq != 1)
    {
      qDebug() << "Not our application based on first:" << mInfo->cSeq;
    }
  }
  // if this is not the first message of this call
  else
  {
    qDebug() << "Existing Call-ID detected.";
    std::shared_ptr<SIPSessionInfo> info = session_;

    // this would be our problem
    Q_ASSERT(info->callID == mInfo->callID);

    if(!mInfo->localTag.isEmpty() && mInfo->localTag != info->localTag &&
       STRICTSIPPROCESSING)
    {
      qDebug() << "Our tag has changed! Something went wrong on other end.";
      return false;
    }

    if(!info->remoteTag.isEmpty() && mInfo->remoteTag != info->remoteTag &&
       STRICTSIPPROCESSING)
    {
      qDebug() << "They have changed their tag! Something went wrong. Old:" << info->remoteTag
               << "new:" << mInfo->remoteTag;
      return false;
    }

    if(mInfo->cSeq != 1)
    {
      qDebug() << "CSeq differs from 1 Got:" << mInfo->cSeq;
    }

    if(info->host != mInfo->host)
    {
      qDebug() << "They changed the host!!";
      return false;
    }

    if(info->connectionID != connectionID)
    {
      qWarning() << "WARNING: Unexpected change of connection ID";
      qDebug() << "Maybe we are calling ourselves";
    }

    // TODO  Maybe check the name portion also
  }

  // check connection details
  connectionMutex_.lock();
  if(connections_.at(connectionID - 1)->connected())
  {
    if(mInfo->localLocation != connections_.at(connectionID - 1)->getLocalAddress().toString())
    {
      qDebug() << "We are not connected from our address:" << mInfo->localLocation;
      return false;
    }
    if(mInfo->remoteLocation != connections_.at(connectionID - 1)->getPeerAddress().toString())
    {
      qDebug() << "We are not connected to their address:" << mInfo->remoteLocation;
      return false;
    }
  }
  connectionMutex_.unlock();

  return true;
}

void SIPState::processRequest(std::shared_ptr<SIPMessageInfo> mInfo,
                                     std::shared_ptr<SDPMessageInfo> peerSDP,
                                     quint32 connectionID)
{
  switch(mInfo->request)
  {
    case INVITE:
    {
      qDebug() << "Found INVITE";

      if(peerSDP == NULL)
      {
        qDebug() << "No SDP received in INVITE";
        // TODO: send malformed request
        return;
      }
      if(session_->contact.contactAddress
         == session_->localAddress.toString())
      {
        emit callingOurselves(mInfo->callID, peerSDP);
      }
      else
      {
        if(peerSDP)
        {
          if(suitableSDP(peerSDP))
          {
            session_->peerSDP = peerSDP;
            emit incomingINVITE(mInfo->callID, session_->contact.name);
          }
          else
          {
            qDebug() << "Could not find suitable SDP parameters within INVITE. Terminating..";
            // TODO implement this response
            sendResponse(UNSUPPORTED_413, session_);
          }
        }
        else
        {
          qDebug() << "No sdpInfo received in request!";
          sendResponse(MALFORMED_400, session_);
        }
      }
      break;
    }
    case ACK:
    {
      qDebug() << "Found ACK";
      if(session_->peerSDP)
      {
        emit callNegotiated(mInfo->callID, session_->peerSDP,
            session_->localSDP);
      }
      else
      {
        qDebug() << "Got ACK without a previous SDP";
      }
      break;
    }
    case BYE:
    {
      qDebug() << "Found BYE";

      sendResponse(OK_200, session_);
      emit callEnded(mInfo->callID, mInfo->replyAddress);

      if(connectionID != session_->connectionID)
      {
        stopConnection(connectionID);
      }
      uninitSession(session_);

      break;
    }
    default:
    {
      qWarning() << "WARNING: Request not implemented";
      break;
    }
  }
}

void SIPState::processResponse(std::shared_ptr<SIPMessageInfo> mInfo,
                                      std::shared_ptr<SDPMessageInfo> peerSDP)
{
  switch(mInfo->response)
  {
    case OK_200:
    {
      qDebug() << "Found 200 OK";
      if(session_->originalRequest == INVITE)
      {
        if(peerSDP)
        {
          if(suitableSDP(peerSDP))
          {
            session_->peerSDP = peerSDP;
            emit ourCallAccepted(mInfo->callID, session_->peerSDP,
                session_->localSDP);
            sendRequest(ACK, session_);
          }
          else
          {
            qDebug() << "SDP not supported";
            // TODO implement this response
            sendResponse(UNSUPPORTED_413, session_);
          }
        }
        else
        {
          qDebug() << "No sdpInfo received in INVITE OK response!";
          sendResponse(MALFORMED_400, session_);
        }

      }
      else if(session_->originalRequest == BYE)
      {
        qDebug() << "They accepted our BYE";
      }
      else if(session_->originalRequest != NOREQUEST)
      {
        qWarning() << "WARNING: Response processing not implemented for this request:"
                   << session_->originalRequest;
      }
      else
      {
        qDebug() << "Received a response for unrecognized request";
      }
      break;
    }
    case DECLINE_603:
    {
      if(session_->originalRequest == INVITE)
      {
        qDebug() << "Received DECLINE";
        emit ourCallRejected(mInfo->callID);
      }
      else
      {
        qDebug() << "Unimplemented decline for:" << session_->originalRequest;
      }
      break;
    }
    default:
    {
      qWarning() << "WARNING: Response processing not implemented!";
    }
  }
}

bool SIPState::suitableSDP(std::shared_ptr<SDPMessageInfo> peerSDP)
{
  Q_ASSERT(peerSDP);
  if(peerSDP == NULL)
  {
    return false;
  }

  // TODO check codec

  //checks if the stream has both audio and video. TODO: Both are actually not necessary
  bool audio = false;
  bool video = false;

  for(auto media : peerSDP->media)
  {
    if(media.type == "video")
    {
      video = true;
    }
    if(media.type == "audio")
    {
      audio = true;
    }
  }

  return audio && video;
}

void SIPState::endAllCalls()
{
  sessionMutex_.lock();
  sendRequest(BYE, session_);
  uninitSession(session_);
  sessionMutex_.unlock();
}

void SIPState::stopConnection(quint32 connectionID)
{
  if(connectionID != 0
     && connections_.at(connectionID - 1) != 0
     && connections_.size() > connectionID - 1 )
  {
    connections_.at(connectionID - 1)->exit(0); // stops qthread
    connections_.at(connectionID - 1)->stopConnection(); // exits run loop
    while(connections_.at(connectionID - 1)->isRunning())
    {
      qSleep(5);
    }
    delete connections_.at(connectionID - 1);
    connections_.at(connectionID - 1) = 0;
  }
}

void SIPState::uninitSession(std::shared_ptr<SIPSessionInfo> info)
{
  stopConnection(info->connectionID);
}

QList<QHostAddress> parseIPAddress(QString address)
{
  QList<QHostAddress> ipAddresses;

  QRegularExpression re("\\b((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)(\\.|$)){4}\\b");
  if(re.match(address).hasMatch())
  {
    qDebug() << "Found IPv4 address:" << address;
    ipAddresses.append(QHostAddress(address));
  }
  else
  {
    qDebug() << "Did not find IPv4 address:" << address;
    QHostInfo hostInfo;
    hostInfo.setHostName(address);

    ipAddresses.append(hostInfo.addresses());
  }
  return ipAddresses;
}

