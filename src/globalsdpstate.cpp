#include "globalsdpstate.h"

#include <QDateTime>

GlobalSDPState::GlobalSDPState():
  localAddress_(),
  localUsername_(""),
  remainingPorts_(0)
{}

void GlobalSDPState::setLocalInfo(QHostAddress localAddress, QString username)
{
  localAddress_ = localAddress;
  localUsername_ = username;
}

void GlobalSDPState::setPortRange(uint16_t minport, uint16_t maxport, uint16_t maxRTPConnections)
{
  for(unsigned int i = minport; i < maxport; i += 2)
  {
    makePortPairAvailable(i);
  }

  remainingPorts_ = maxRTPConnections;
}

std::shared_ptr<SDPMessageInfo> GlobalSDPState::localInviteSDP()
{
  return generateSDP();
}

std::shared_ptr<SDPMessageInfo> GlobalSDPState::generateSDP()
{
  if(localAddress_ == QHostAddress::Null
     || localUsername_ == "")
  {
    qWarning() << "WARNING: Necessary info not set for SDP generation:" << localAddress_.toString()
               << localUsername_;
    return NULL;
  }

  if(availablePorts_.size() <= 2)
  {
    qDebug() << "Not enough free ports to create SDP:" << availablePorts_.size();
    return NULL;
  }

  // TODO: Get suitable SDP from media manager
  std::shared_ptr<SDPMessageInfo> newInfo = std::shared_ptr<SDPMessageInfo> (new SDPMessageInfo);
  newInfo->version = 0;
  newInfo->originator_username = localUsername_;
  newInfo->sess_id = QDateTime::currentMSecsSinceEpoch();
  newInfo->sess_v = QDateTime::currentMSecsSinceEpoch();
  newInfo->host_nettype = "IN";
  if(localAddress_.protocol() == QAbstractSocket::IPv6Protocol)
  {
    newInfo->host_addrtype = "IP6";
    newInfo->connection_addrtype = "IP6";
  }
  else
  {
    newInfo->host_addrtype = "IP4";
    newInfo->connection_addrtype = "IP4";
  }

  newInfo->host_address = localAddress_.toString();

  newInfo->sessionName = " ";
  newInfo->sessionDescription = "A Kvazzup Video Conference";

  newInfo->connection_address = localAddress_.toString();
  newInfo->connection_nettype = "IN";

  MediaInfo audio;
  MediaInfo video;
  audio.type = "audio";
  video.type = "video";
  audio.receivePort = nextAvailablePortPair();
  video.receivePort = nextAvailablePortPair();
  if(audio.receivePort == 0 || video.receivePort == 0)
  {
    makePortPairAvailable(audio.receivePort);
    makePortPairAvailable(video.receivePort);
    qDebug() << "WARNING: Ran out of ports to assign to SDP. SHould be checked earlier.";
    return NULL;
  }
  audio.proto = "RTP/AVP";
  video.proto = "RTP/AVP";
  audio.rtpNum = 96;
  video.rtpNum = 97;
  // we ignore nettype, addrtype and address, because we have a global c=

  RTPMap a_rtp;
  RTPMap v_rtp;
  a_rtp.rtpNum = 96;
  v_rtp.rtpNum = 97;
  a_rtp.clockFrequency = 48000;
  v_rtp.clockFrequency = 90000;
  a_rtp.codec = "opus";
  v_rtp.codec = "h265";

  audio.codecs.push_back(a_rtp);
  video.codecs.push_back(a_rtp);
  audio.activity = SENDRECV;
  video.activity = SENDRECV;

  newInfo->media.push_back(audio);
  newInfo->media.push_back(video);

  qDebug() << "SDP generated";

  return newInfo;
}
// returns NULL if suitable could not be found
std::shared_ptr<SDPMessageInfo> GlobalSDPState::localResponseSDP(std::shared_ptr<SDPMessageInfo> remoteInviteSDP)
{
  if(remoteInviteSDP == NULL)
  {
    qDebug() << "WARNING: Got a remote NULL invite SDP.";
  }
  else if(!checkSDPOffer(remoteInviteSDP))
  {
    qDebug() << "Incoming SDP did not have Opus and H265 in their offer.";
    return NULL;
  }

  // check if suitable.
  // Generate response
  return generateSDP();
}

bool GlobalSDPState::remoteFinalSDP(std::shared_ptr<SDPMessageInfo> remoteInviteSDP)
{
  return checkSDPOffer(remoteInviteSDP);
}

bool GlobalSDPState::checkSDPOffer(std::shared_ptr<SDPMessageInfo> offer)
{
  bool hasOpus = false;
  bool hasH265 = false;

  for(MediaInfo media : offer->media)
  {
    for(RTPMap rtp : media.codecs)
    {
      if(rtp.codec == "opus")
      {
        qDebug() << "Found Opus";
        hasOpus = true;
      }
      else if(rtp.codec == "h265")
      {
        qDebug() << "Found H265";
        hasH265 = true;
      }
    }
  }

  return hasOpus && hasH265;
}

uint16_t GlobalSDPState::nextAvailablePortPair()
{
  uint16_t newLowerPort = 0;

  portLock_.lock();
  if(availablePorts_.size() <= 1 && remainingPorts_ >= 2)
  {
    newLowerPort = availablePorts_.at(0);
    availablePorts_.pop_front();
    remainingPorts_ -= 2;
  }
  portLock_.unlock();

  return newLowerPort;
}

void GlobalSDPState::makePortPairAvailable(uint16_t lowerPort)
{
  if(lowerPort != 0)
  {
    portLock_.lock();
    availablePorts_.push_back(lowerPort);
    remainingPorts_ += 2;
    portLock_.unlock();

  }
}
