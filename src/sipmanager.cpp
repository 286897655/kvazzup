#include "sipmanager.h"

#include "siprouting.h"
#include "sipsession.h"
#include "sipconnection.h"

const bool DIRECTMESSAGES = false;


SIPManager::SIPManager():
  isConference_(false),
  sipPort_(5060), // default for SIP, use 5061 for tls encrypted
  localName_(""),
  localUsername_("")
{}

void SIPManager::init()
{
  qDebug() << "Iniating SIP Manager";

  QObject::connect(&server_, SIGNAL(newConnection(TCPConnection*)),
                   this, SLOT(receiveTCPConnection(TCPConnection*)));

  // listen to everything
  server_.listen(QHostAddress("0.0.0.0"), sipPort_);

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
  sdp_.setLocalInfo(QHostAddress("0.0.0.0"), localUsername_);
  sdp_.setPortRange(21500, 22000, 42);
}

void SIPManager::uninit()
{}

QList<uint32_t> SIPManager::startCall(QList<Contact> addresses)
{
  QList<uint32_t> calls;
  for(unsigned int i = 0; i < addresses.size(); ++i)
  {
    SIPDialogData* dialog = new SIPDialogData;
    dialog->sCon = new SIPConnection(dialogs_.size() + 1);

    dialogMutex_.lock();
    dialog->session = createSIPSession(dialogs_.size() + 1);
    dialog->routing = new SIPRouting;
    dialog->routing->setLocalNames(localUsername_, localName_);
    dialog->routing->setRemoteUsername(addresses.at(i).username);
    dialog->hostedSession = false;

    //dialog->callID = dialog->session->startCall();

    QObject::connect(dialog->sCon, SIGNAL(sipConnectionEstablished(quint32,QString,QString)),
                     this, SLOT(connectionEstablished(quint32,QString,QString)));

    // message is sent only after connection has been established so we know our address
    dialog->sCon->initConnection(TCP, addresses.at(i).remoteAddress);

    dialogs_.push_back(dialog);
    calls.push_back(dialogs_.size());
    dialogMutex_.unlock();
    qDebug() << "Added a new dialog:" << dialogs_.size();
  }

  return calls;
}

void SIPManager::acceptCall(uint32_t sessionID)
{

}

void SIPManager::rejectCall(uint32_t sessionID)
{

}

void SIPManager::endCall(uint32_t sessionID)
{

}

void SIPManager::endAllCalls()
{

}

SIPSession *SIPManager::createSIPSession(uint32_t sessionID)
{
  SIPSession* session = new SIPSession();

  session->init(sessionID);

  QObject::connect(session, SIGNAL(sendRequest(uint32_t,RequestType)),
                   this, SLOT(sendRequest(uint32_t,RequestType)));

  QObject::connect(session, SIGNAL(sendResponse(uint32_t,ResponseType)),
                   this, SLOT(sendResponse(uint32_t,ResponseType)));

  // connect signals to signals. Session is responsible for responses
  // and callmanager handles informing the user.
  return session;
}

void SIPManager::receiveTCPConnection(TCPConnection *con)
{
  qDebug() << "Received a TCP connection. Initializing a connection";
  Q_ASSERT(con);
  SIPDialogData* dialog = new SIPDialogData;
  dialog->sCon = new SIPConnection(dialogs_.size() + 1);
  dialog->sCon->incomingTCPConnection(std::shared_ptr<TCPConnection> (con));
  // note: the connection has already been established
  dialog->session = NULL;
  dialog->routing = NULL;

  dialogs_.append(dialog);
}

// connection has been established. This enables for us to get the needed info
// to form a SIP message
void SIPManager::connectionEstablished(quint32 sessionID, QString localAddress, QString remoteAddress)
{
  if(sessionID == 0 || sessionID> dialogs_.size())
  {
    qDebug() << "WARNING: Missing session for connected session";
    return;
  }
  SIPDialogData* dialog = dialogs_.at(sessionID - 1);

  if(dialog->hostedSession)
  {
    qDebug() << "Setting connection as a SIP server connection.";
    dialog->routing->setLocalAddress(localAddress);
    dialog->routing->setLocalHost(remoteAddress);

    // TODO enable remote to have a different host from ours
    dialog->routing->setRemoteHost(remoteAddress);
  }
  else
  {
    qDebug() << "Setting connection as a peer-to-peer SIP connection. Firewall needs to be open for this";
    dialog->routing->setLocalAddress(localAddress);
    dialog->routing->setLocalHost(localAddress);
    dialog->routing->setRemoteHost(remoteAddress);
  }

  qDebug() << "Setting info for peer-to-peer connection";
  // TODO make possible to set the server

  qDebug() << "Connection" << sessionID << "connected."
           << "From:" << localAddress
           << "To:" << remoteAddress;

  if(!dialog->session->startCall())
  {
    qDebug() << "WARNING: Could not start a call according to session.";
  }
}

void SIPManager::processSIPRequest(RequestType request, std::shared_ptr<SIPRoutingInfo> routing,
                       std::shared_ptr<SIPSessionInfo> session,
                       std::shared_ptr<SIPMessageInfo> message,
                       quint32 sessionID)
{
  if(!dialogs_.at(sessionID - 1)->routing->incomingSIPRequest(routing))
  {
    qDebug() << "Something wrong with incoming SIP request";
    // TODO: sent to wrong address
    return;
  }

  qWarning() << "WARNING: Processing requests not implemented yet";
  //dialogs_.at(sessionID - 1)->session->processRequest();
}

void SIPManager::processSIPResponse(ResponseType response, std::shared_ptr<SIPRoutingInfo> routing,
                        std::shared_ptr<SIPSessionInfo> session,
                        std::shared_ptr<SIPMessageInfo> message, quint32 sessionID)
{
  if(!dialogs_.at(sessionID - 1)->routing->incomingSIPResponse(routing))
  {

    qDebug() << "Something wrong with incoming SIP response";
    // TODO: sent to wrong address
    return;
  }
  qWarning() << "WARNING: Processing responses not implemented yet";
}

void SIPManager::sendRequest(uint32_t sessionID, RequestType type)
{
  qDebug() << "---- Iniated sending of a request ---";
  Q_ASSERT(sessionID <= dialogs_.size());
  QString directRouting = "";

  // this is a bit confusing. Get all the necessary information from different components.
  SIPRequest request = {type, dialogs_.at(sessionID - 1)->session->generateMessage(type)};
  request.message->routing = dialogs_.at(sessionID - 1)->routing->requestRouting(directRouting);
  request.message->session = dialogs_.at(sessionID - 1)->session->getRequestInfo();

  std::shared_ptr<SDPMessageInfo> sdp_info = NULL;
  if(type == INVITE)
  {
    sdp_info = sdp_.localInviteSDP();
  }

  dialogs_.at(sessionID - 1)->sCon->sendRequest(request, sdp_info);
  qDebug() << "---- Finished sending of a request ---";
}

void SIPManager::sendResponse(uint32_t sessionID, ResponseType response)
{
  qDebug() << "WARNING: Sending responses not implemented yet";


}

void SIPManager::destroySession(SIPDialogData *dialog)
{
  if(dialog != NULL)
  {
    if(dialog->sCon != NULL)
    {
      dialog->sCon->destroyConnection();
    }

    if(dialog->session != NULL)
    {
      //dialog->session->uninit();
      delete dialog->session;
      dialog->session = NULL;
    }
  }
}
