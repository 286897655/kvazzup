#pragma once

#include <QHostAddress>
#include <QString>
#include <QWaitCondition>
#include <memory>

#include "sdptypes.h"
#include "icetypes.h"
#include "networkcandidates.h"

class FlowAgent;

class ICE : public QObject
{
  Q_OBJECT

  public:
    ICE();
    ~ICE();

    // generate a list of local candidates for media streaming
    QList<std::shared_ptr<ICEInfo>>
        generateICECandidates(std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> localCandidates,
                              std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> globalCandidates,
                              std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> stunCandidates,
                              std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> turnCandidates);

    // Call this function to start the connectivity check/nomination process.
    // Does not block
    void startNomination(QList<std::shared_ptr<ICEInfo>>& local,
                         QList<std::shared_ptr<ICEInfo>>& remote,
                         uint32_t sessionID, bool flowController);

    // get nominated ICE pair using sessionID
    ICEMediaInfo getNominated(uint32_t sessionID);

    // free all ICE-related resources
    void cleanupSession(uint32_t sessionID);

signals:
    void nominationFailed(quint32 sessionID);
    void nominationSucceeded(quint32 sessionID);



private slots:
    // when FlowAgent has finished its job, it emits "ready" signal which is caught by this slot function
    // handleCallerEndOfNomination() check if the nomination succeeed, saves the nominated pair to hashmap and
    // releases caller_mtx to signal that negotiation is done
    // save nominated pair to hashmap so it can be fetched later on
    void handleEndOfNomination(std::shared_ptr<ICEPair> rtp,
                               std::shared_ptr<ICEPair> rtcp, uint32_t sessionID);

  private:
    // create media candidate (RTP and RTCP connection)
    // "type" marks whether this candidate is host or server reflexive candidate (affects priority)
    std::pair<std::shared_ptr<ICEInfo>,
              std::shared_ptr<ICEInfo>> makeCandidate(const std::pair<QHostAddress, uint16_t>& addressPort,
                                                      QString type);

    int calculatePriority(int type, int local, int component);

    void printCandidate(ICEInfo *candidate);



    // makeCandidatePairs takes a list of local and remote candidates, matches them based on localilty (host/server-reflexive)
    // and component (RTP/RTCP) and returns a list of ICEPairs used for connectivity checks
    QList<std::shared_ptr<ICEPair>> makeCandidatePairs(QList<std::shared_ptr<ICEInfo>>& local,
                                                       QList<std::shared_ptr<ICEInfo>>& remote);

    void addCandidates(std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> addresses,
                       const QString& candidatesType,
                       QList<std::shared_ptr<ICEInfo>>& candidates);

    bool nominatingConnection_;

    struct NominationInfo
    {
      FlowAgent *agent;

      // list of all candidates, remote and local
      QList<std::shared_ptr<ICEPair>> pairs;

      std::pair<
        std::shared_ptr<ICEPair>,
        std::shared_ptr<ICEPair>
      > nominatedVideo;

      std::pair<
        std::shared_ptr<ICEPair>,
        std::shared_ptr<ICEPair>
      > nominatedAudio;

      bool connectionNominated;
    };

    // key is sessionID
    QMap<uint32_t, struct NominationInfo> nominationInfo_;
};
