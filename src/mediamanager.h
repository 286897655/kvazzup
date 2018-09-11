#pragma once

#include <QObject>
#include <QMutex>

#include <memory>
#include <map>

#ifndef _MSC_VER
#include <inaddr.h>
#else
#include <WinSock2.h>
#endif

// Manages the Media delivery and Media Processing and the interaction between the two
// especially during construction.

class VideoviewFactory;
class StatisticsInterface;

class FilterGraph;
class RTPStreamer;
class MediaSession;
struct SDPMessageInfo;
struct MediaInfo;

typedef int16_t PeerID;

class MediaManager : public QObject
{
  Q_OBJECT

public:
  MediaManager();
  ~MediaManager();

  void init(std::shared_ptr<VideoviewFactory> viewfactory, StatisticsInterface *stats);
  void uninit();

  void updateSettings();

  // registers a contact for activity monitoring
  void registerContact(in_addr ip);

  void addParticipant(uint32_t sessionID, const std::shared_ptr<SDPMessageInfo> peerInfo,
                      const std::shared_ptr<SDPMessageInfo> localInfo);

  void removeParticipant(uint32_t sessionID);
  void endAllCalls();

  // Functions that enable using Kvazzup as just a streming client for whatever reason.
  void streamToIP(in_addr ip, uint16_t port);
  void receiveFromIP(in_addr ip, uint16_t port);

  // call changes. Returns state after toggle
  bool toggleMic();
  bool toggleCamera();

signals:

  // somebody is calling
  void incomingCall(unsigned int participantID);

  // our call was not accepted
  void callRejected(unsigned int participantID);

  // somebody muted their mic
  void mutedMic(unsigned int participantID);

  // somebody left the call we are in
  void participantDropped(unsigned int participantID);

  // Somebody came online or went offline
  void statusChanged(unsigned int participantID, bool online);

  // the host has quit the call and we have been chosen to become the new host (ability to kick people)
  void becameHost();

private:

  void createOutgoingMedia(uint32_t sessionID, const MediaInfo& media);
  void createIncomingMedia(uint32_t sessionID, const MediaInfo& media);

  StatisticsInterface* stats_;

  std::unique_ptr<FilterGraph> fg_;

  MediaSession* session_;

  std::unique_ptr<RTPStreamer> streamer_;

  std::shared_ptr<VideoviewFactory> viewfactory_;

  bool mic_;
  bool camera_;
};
