#ifndef RTPSTREAMER_H
#define RTPSTREAMER_H

#include "framedsourcefilter.h"
#include "rtpsinkfilter.h"
#include "filter.h"

#include <liveMedia.hh>
#include <UsageEnvironment.hh>
#include <GroupsockHelper.hh>

#include <vector>

class FramedSourceFilter;
class RTPSinkFilter;

typedef int16_t PeerID;

class RTPStreamer : public QThread
{
  Q_OBJECT

public:
  RTPStreamer(StatisticsInterface* stats);

  void run();
  void stop();

  // use these to ask for filters that are connected to filter graph
  FramedSourceFilter* getSendFilter(PeerID peer, DataType type);
  RTPSinkFilter* getReceiveFilter(PeerID peer, DataType type);

  // if audioPort or videoPort is 0, corresponding streams are not created
  PeerID addPeer(in_addr peerAddress, uint16_t videoPort, uint16_t audioPort);

  // removes everything related to this peer
  void removePeer(PeerID id);

private:

  struct Connection
  {
    Port* rtpPort;
    Port* rtcpPort;
    Groupsock* rtpGroupsock;
    Groupsock* rtcpGroupsock;
  };

  struct Sender
  {
    Connection connection;

    RTCPInstance* rtcp;

    RTPSink* sink;
    FramedSourceFilter* framedSource; // receives stuff from filter graph
  };

  struct Receiver
  {
    Connection connection;

    RTCPInstance* rtcp;

    RTPSource* framedSource;
    RTPSinkFilter* sink; // sends stuff to filter graph
  };

  struct Peer
  {
    PeerID id;
    struct in_addr ip;
    Sender* audioSender; // audio to this peer
    Sender* videoSender; // video to this peer

    Receiver* audioReceiver; // audio from this peer
    Receiver* videoReceiver; // video from this peer
  };

  void initLiveMedia();

  void createConnection(Connection& connection,
                        struct in_addr ip, uint16_t portNum,
                        bool reservePorts);

  void destroyConnection(Connection& connection);

  Sender* addSender(in_addr ip, uint16_t port, DataType type);
  Receiver* addReceiver(in_addr peerAddress, uint16_t port, DataType type);

  void destroySender(Sender* sender);
  void destroyReceiver(Receiver* recv);

  void uninit();

  std::vector<Peer*> peers_;

  QMutex iniated_;
  QMutex peer_;

  uint8_t ttl_;
  struct in_addr sessionAddress_;

  char stopRTP_; // char for stopping live555 taskscheduler
  UsageEnvironment* env_;

  StatisticsInterface* stats_;

  static const unsigned int maxCNAMElen_ = 100;
  unsigned char CNAME_[maxCNAMElen_ + 1];

};

#endif // RTPSTREAMER_H
