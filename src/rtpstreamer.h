#ifndef RTPSTREAMER_H
#define RTPSTREAMER_H

#include "framedsourcefilter.h"
#include "rtpsinkfilter.h"
#include "filter.h"

#include <liveMedia.hh>
#include <UsageEnvironment.hh>
#include <GroupsockHelper.hh>


class FramedSourceFilter;
class RTPSinkFilter;

typedef uint16_t PeerID;


class RTPStreamer : public QThread
{
  Q_OBJECT

public:
  RTPStreamer();

  //void setDestination(in_addr address, uint16_t port);

  void run();

  void stop();

  FramedSourceFilter* getSourceFilter(PeerID peer)
  {
    if(iniated_)
      return senders_[peer];
    return NULL;
  }

  RTPSinkFilter* getSinkFilter(PeerID peer)
  {
    if(iniated_)
      return receivers_[peer];
    return NULL;
  }

  PeerID addPeer(in_addr peerAddress, bool video, bool audio = true);

  void removePeer(PeerID id);

private:

  void initLiveMedia();
  void addH265VideoSend(PeerID peer, in_addr peerAddress);
  void addH265VideoReceive(PeerID peer, in_addr peerAddress);
  void uninit();

  struct Sender
  {
    struct in_addr peerAddress;

    Port* rtpPort;
    Port* rtcpPort;
    Groupsock* rtpGroupsock;
    Groupsock* rtcpGroupsock;

    RTCPInstance* rtcp;

    RTPSink* videoSink;
    FramedSourceFilter* videoSource; // receives stuff from filter graph
  };

  struct Receiver
  {
    struct in_addr peerAddress;

    Port* rtpPort;
    //Port* rtcpPort_;
    Groupsock* rtpGroupsock;
    //Groupsock* rtcpGroupsock_;

    FramedSource* videoSource;
    RTPSinkFilter* videoSink; // sends stuff to filter graph
  };

  std::map<PeerID, Sender> senders_;
  std::map<PeerID, Receiver> receivers_;

  PeerID nextID_;
  bool iniated_;
  uint16_t portNum_;
  uint8_t ttl_;
  struct in_addr sessionAddress_;

  char stopRTP_;
  UsageEnvironment* env_;

};

#endif // RTPSTREAMER_H
