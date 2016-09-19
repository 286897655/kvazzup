#include "rtpstreamer.h"

#include "framedsourcefilter.h"

#include <liveMedia.hh>
#include <UsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <BasicUsageEnvironment.hh>

#include <QtEndian>
#include <QHostInfo>
#include <QDebug>

#include <iostream>

RTPStreamer::RTPStreamer(StatisticsInterface* stats):
  peers_(),
  iniated_(),
  peer_(),
  ttl_(255),
  sessionAddress_(),
  stopRTP_(0),
  env_(NULL),
  stats_(stats),
  CNAME_()
{
  // use unicast
  QString ip_str = "0.0.0.0";
  QHostAddress address;
  address.setAddress(ip_str);
  sessionAddress_.S_un.S_addr = qToBigEndian(address.toIPv4Address());

  gethostname((char*)CNAME_, maxCNAMElen_);
  CNAME_[maxCNAMElen_] = '\0'; // just in case

  iniated_.lock();
}

void RTPStreamer::run()
{
  qDebug() << "Iniating RTP streamer";
  initLiveMedia();
  iniated_.unlock();
  qDebug() << "Iniating RTP streamer finished. "
           << "RTP streamer starting eventloop";

  stopRTP_ = 0;
  env_->taskScheduler().doEventLoop(&stopRTP_);

  qDebug() << "RTP streamer eventloop stopped";

  uninit();

}

void RTPStreamer::stop()
{
  stopRTP_ = 1;
}

void RTPStreamer::uninit()
{
  Q_ASSERT(stopRTP_);
  qDebug() << "Uniniating RTP streamer";

  for(unsigned int i = 0; i < peers_.size(); ++i)
  {
    if(peers_.at(i) != 0)
    {
      removePeer(i);
    }
  }

  if(!env_->reclaim())
    qWarning() << "Unsuccesful reclaim of usage environment";

  qDebug() << "RTP streamer uninit succesful";
}

void RTPStreamer::initLiveMedia()
{
  qDebug() << "Iniating live555";
  QString sName(reinterpret_cast<char*>(CNAME_));
  qDebug() << "Our hostname:" << sName;

  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  if(scheduler)
    env_ = BasicUsageEnvironment::createNew(*scheduler);

  OutPacketBuffer::maxSize = 65536;
}

PeerID RTPStreamer::addPeer(in_addr ip)
{
  iniated_.lock();
  peer_.lock();
  qDebug() << "Adding peer to following IP: "
           << (uint8_t)((ip.s_addr) & 0xff) << "."
           << (uint8_t)((ip.s_addr >> 8) & 0xff) << "."
           << (uint8_t)((ip.s_addr >> 16) & 0xff) << "."
           << (uint8_t)((ip.s_addr >> 24) & 0xff);

  Peer* peer = new Peer;

  peer->ip = ip;
  peer->videoSender = 0;
  peer->videoReceiver = 0;
  peer->audioSender = 0;
  peer->audioReceiver = 0;

  peers_.push_back(peer);

  peer_.unlock();
  iniated_.unlock();

  qDebug() << "RTP streamer: Peer #" << peers_.size() - 1 << "added";

  return (PeerID)peers_.size() - 1;
}

void RTPStreamer::removePeer(PeerID id)
{
  if(peers_.at(id) != 0)
  {
    if(peers_.at(id)->audioSender)
      destroySender(peers_.at(id)->audioSender);
    if(peers_.at(id)->videoSender)
      destroySender(peers_.at(id)->videoSender);
    if(peers_.at(id)->audioReceiver)
      destroyReceiver(peers_.at(id)->audioReceiver);
    if(peers_.at(id)->videoReceiver)
      destroyReceiver(peers_.at(id)->videoReceiver);

    delete peers_.at(id);

    peers_.at(id) = 0;
  }
}

void RTPStreamer::destroySender(Sender* sender)
{
  Q_ASSERT(sender);
  if(sender)
  {
    RTCPInstance::close(sender->rtcp);
    sender->framedSource = NULL;
    sender->sink->stopPlaying();
    RTPSink::close(sender->sink);
    destroyConnection(sender->connection);

    delete sender;
  }
  else
    qWarning() << "Warning: Tried to delete sender a second time";
}
void RTPStreamer::destroyReceiver(Receiver* recv)
{
  Q_ASSERT(recv);
  if(recv)
  {
    RTCPInstance::close(recv->rtcp);
    recv->sink->stopPlaying();
    recv->sink = NULL; // deleted in filter graph
    FramedSource::close(recv->framedSource);
    recv->framedSource = 0;
    destroyConnection(recv->connection);

    delete recv;
  }
  else
    qWarning() << "Warning: Tried to delete receiver a second time";
}

void RTPStreamer::addSendVideo(PeerID peer, uint16_t port)
{
  if(peers_.at(peer)->videoSender)
    destroySender(peers_.at(peer)->videoSender);

  peers_.at(peer)->videoSender = addSender(peers_.at(peer)->ip, port, HEVCVIDEO);
}

void RTPStreamer::addSendAudio(PeerID peer, uint16_t port)
{
  if(peers_.at(peer)->audioSender)
    destroySender(peers_.at(peer)->audioSender);

  peers_.at(peer)->audioSender = addSender(peers_.at(peer)->ip, port, RAWAUDIO);
}


void RTPStreamer::addReceiveVideo(PeerID peer, uint16_t port)
{
  if(peers_.at(peer)->videoReceiver)
    destroyReceiver(peers_.at(peer)->videoReceiver);

  peers_.at(peer)->videoReceiver = addReceiver(peers_.at(peer)->ip, port, HEVCVIDEO);
}

void RTPStreamer::addReceiveAudio(PeerID peer, uint16_t port)
{
  if(peers_.at(peer)->audioReceiver)
    destroyReceiver(peers_.at(peer)->audioReceiver);

  peers_.at(peer)->audioReceiver = addReceiver(peers_.at(peer)->ip, port, RAWAUDIO);
}

void RTPStreamer::removeSendVideo(PeerID peer)
{
  if(peers_.at(peer)->videoSender)
  {
    destroySender(peers_.at(peer)->videoSender);
    peers_.at(peer)->videoSender = 0;
  }
  else
    qWarning() << "WARNING: Tried to remove send video that did not exist.";
}
void RTPStreamer::removeSendAudio(PeerID peer)
{
  if(peers_.at(peer)->audioSender)
  {
    destroySender(peers_.at(peer)->audioSender);
    peers_.at(peer)->audioSender = 0;
  }

  else
    qWarning() << "WARNING: Tried to remove send video that did not exist.";
}

void RTPStreamer::removeReceiveVideo(PeerID peer)
{
  if(peers_.at(peer)->videoReceiver)
  {
    destroyReceiver(peers_.at(peer)->videoReceiver);
    peers_.at(peer)->videoReceiver = 0;
  }
  else
    qWarning() << "WARNING: Tried to remove send video that did not exist.";
}
void RTPStreamer::removeReceiveAudio(PeerID peer)
{
  if(peers_.at(peer)->audioReceiver)
  {
    destroyReceiver(peers_.at(peer)->audioReceiver);
    peers_.at(peer)->audioReceiver = 0;
  }
  else
    qWarning() << "WARNING: Tried to remove send video that did not exist.";
}

RTPStreamer::Sender* RTPStreamer::addSender(in_addr ip, uint16_t port, DataType type)
{
  qDebug() << "Iniating send RTP/RTCP stream";

  Sender* sender = new Sender;
  createConnection(sender->connection, ip, port, false);

  // todo: negotiate payload number
  switch(type)
  {
  case HEVCVIDEO :
    sender->sink = H265VideoRTPSink::createNew(*env_, sender->connection.rtpGroupsock, 96);
    break;
  case RAWAUDIO :
    sender->sink = SimpleRTPSink::createNew(*env_, sender->connection.rtpGroupsock, 0,
                                            48000, "audio", "PCM", 2, False);
    break;
  case OPUSAUDIO :
    sender->sink = SimpleRTPSink::createNew(*env_, sender->connection.rtpGroupsock, 97,
                                            48000, "audio", "OPUS", 2, False);
    break;
  default :
    qWarning() << "Warning: RTP support not implemented for this format";
    break;
  }
  sender->framedSource = new FramedSourceFilter(stats_, *env_, type);
  const unsigned int estimatedSessionBandwidth = 5000; // in kbps; for RTCP b/w share
  // This starts RTCP running automatically
  sender->rtcp  = RTCPInstance::createNew(*env_,
                                   sender->connection.rtcpGroupsock,
                                   estimatedSessionBandwidth,
                                   CNAME_,
                                   sender->sink,
                                   NULL,
                                   False);

  if(!sender->framedSource || !sender->sink)
  {
    qCritical() << "Failed to setup sending RTP stream";
    return NULL;
  }

  if(!sender->sink->startPlaying(*(sender->framedSource), NULL, NULL))
  {
    qCritical() << "failed to start videosink: " << env_->getResultMsg();
  }
  return sender;
}

RTPStreamer::Receiver* RTPStreamer::addReceiver(in_addr peerAddress, uint16_t port, DataType type)
{
  qDebug() << "Iniating receive RTP/RTCP stream";
  Receiver *receiver = new Receiver;
  createConnection(receiver->connection, peerAddress, port, true);

  // todo: negotiate payload number
  switch(type)
  {
  case HEVCVIDEO :
    receiver->framedSource = H265VideoRTPSource::createNew(*env_, receiver->connection.rtpGroupsock, 96);
    break;
  case RAWAUDIO :
    receiver->framedSource = SimpleRTPSource::createNew(*env_, receiver->connection.rtpGroupsock, 0,
                                                        48000, "audio/PCM", 0, True);
    break;
  case OPUSAUDIO :
    receiver->framedSource = SimpleRTPSource::createNew(*env_, receiver->connection.rtpGroupsock, 97,
                                                        48000, "audio/OPUS", 0, True);
    break;
  default :
    qWarning() << "Warning: RTP support not implemented for this format";
    break;
  }

  receiver->sink = new RTPSinkFilter(stats_, *env_);

  const unsigned int estimatedSessionBandwidth = 5000; // in kbps; for RTCP b/w share
  // This starts RTCP running automatically
  receiver->rtcp  = RTCPInstance::createNew(*env_,
                                   receiver->connection.rtcpGroupsock,
                                   estimatedSessionBandwidth,
                                   CNAME_,
                                   NULL,
                                   receiver->framedSource, // this is the client
                                   False);

  if(!receiver->framedSource || !receiver->sink)
  {
    qCritical() << "Failed to setup receiving RTP stream";
    RTCPInstance::close(receiver->rtcp);
    destroyConnection(receiver->connection);
    delete receiver;
    return NULL;
  }

  if(!receiver->sink->startPlaying(*receiver->framedSource,NULL,NULL))
  {
    qCritical() << "failed to start videosink: " << env_->getResultMsg();
  }

  return receiver;
}

void RTPStreamer::createConnection(Connection& connection, struct in_addr ip,
                                   uint16_t portNum, bool reservePorts)
{
  if(!reservePorts)
  {
    connection.rtpPort = new Port(0);
    connection.rtcpPort = new Port(0);
  }
  else
  {
    connection.rtpPort = new Port(portNum);
    connection.rtcpPort = new Port(portNum + 1);
  }

  connection.rtpGroupsock = new Groupsock(*env_, sessionAddress_, ip, *connection.rtpPort);
  connection.rtcpGroupsock = new Groupsock(*env_, ip, *(connection.rtcpPort), ttl_);

  if(!reservePorts)
  {
    connection.rtpGroupsock->changeDestinationParameters(ip, portNum, ttl_);
    connection.rtcpGroupsock->changeDestinationParameters(ip, portNum + 1, ttl_);
  }
}

void RTPStreamer::destroyConnection(Connection& connection)
{
  if(connection.rtpGroupsock)
  {
    delete connection.rtpGroupsock;
    connection.rtpGroupsock = 0;
  }
  if(connection.rtcpGroupsock)
  {
    delete connection.rtcpGroupsock;
    connection.rtcpGroupsock = 0;
  }

  if(connection.rtpPort)
  {
    delete connection.rtpPort;
    connection.rtpPort = 0;
  }
  if(connection.rtcpPort)
  {
    delete connection.rtcpPort;
    connection.rtcpPort = 0;
  }
}

// use these to ask for filters that are connected to filter graph
FramedSourceFilter* RTPStreamer::getSendFilter(PeerID peer, DataType type)
{
  peer_.lock();
  // TODO: Check availability
//   Q_ASSERT(videoSenders_.find(peer) != videoSenders_.end());
  peer_.unlock();

  if(type == RAWAUDIO)
    return peers_[peer]->audioSender->framedSource;
  else if(type == HEVCVIDEO)
    return peers_[peer]->videoSender->framedSource;

  return NULL;
}

RTPSinkFilter* RTPStreamer::getReceiveFilter(PeerID peer, DataType type)
{
  peer_.lock();
//    Q_ASSERT(receivers_.find(peer) != receivers_.end());
  peer_.unlock();

  if(type == RAWAUDIO)
    return peers_[peer]->audioReceiver->sink;
  else if(type == HEVCVIDEO)
    return peers_[peer]->videoReceiver->sink;

  return NULL;
}
