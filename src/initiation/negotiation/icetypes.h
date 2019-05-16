#pragma once

#include <memory>

enum PAIR {
  PAIR_WAITING     = 0,
  PAIR_IN_PROGRESS = 1,
  PAIR_SUCCEEDED   = 2,
  PAIR_FAILED      = 3,
  PAIR_FROZEN      = 4,
  PAIR_NOMINATED   = 5,
};

enum COMPONENTS {
  RTP  = 1,
  RTCP = 2
};

/* list of ICEInfo (candidates) is send during INVITE */
struct ICEInfo
{
  QString foundation;  /* TODO:  */
  int component;       /* 1 for RTP, 2 for RTCP */
  QString transport;   /* UDP/TCP */
  int priority;        /* TODO: */

  QString address;
  int port;

  QString type;        /* host/relayed */
  QString rel_address; /* for turn, not used (currently)  */
};

struct ICEPair
{
  std::shared_ptr<ICEInfo> local;
  std::shared_ptr<ICEInfo> remote;
  int priority;
  int state;
};

struct ICEMediaInfo
{
  // first ICEPair is for RTP, second for RTCP
  // ICEPair contains both local and remote address/port pairs (see above)
  std::pair<
    std::shared_ptr<ICEPair>,
    std::shared_ptr<ICEPair>
  > video;

  std::pair<
    std::shared_ptr<ICEPair>,
    std::shared_ptr<ICEPair>
  > audio;
};