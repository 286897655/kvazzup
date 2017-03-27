#pragma once

#include <QList>
#include <QString>
#include <stdint.h>

enum MessageType {NOREQUEST, INVITE, ACK, BYE, CANCEL, OPTIONS, REGISTER, OK200}; // RFC 3261
              //PRACK,SUBSCRIBE, NOTIFY, PUBLISH, INFO, REFER, MESSAGE, UPDATE }; RFC 3262, 6665, 3903, 6086, 3515, 3428, 3311

const bool STRICTSIPPROCESSING = true;

/* SDP message info structs */
struct RTPMap
{
  uint16_t rtpNum;
  uint32_t clockFrequency;
  QString codec;
};

struct MediaInfo
{
  QString type;
  uint16_t port;
  QString proto;
  uint16_t rtpNum;
  QString nettype;
  QString addrtype;
  QString address;

  QList<RTPMap> codecs;
};

struct SDPMessageInfo
{
  uint8_t version;
  QString username;
  uint32_t sess_id;
  uint32_t sess_v;
  QString host_nettype;
  QString host_addrtype;
  QString hostAddress;

  QString sessionName;

  uint32_t startTime;
  uint32_t endTime;

  QString global_nettype;
  QString global_addrtype;
  QString globalAddress;

  QList<MediaInfo> media;
};
