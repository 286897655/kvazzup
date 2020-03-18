#include <QEventLoop>
#include <QTimer>
#include <QThread>

#include "common.h"
#include "connectiontester.h"
#include "flowagent.h"
#include "ice.h"


FlowAgent::FlowAgent(bool controller, int timeout):
  candidates_(nullptr),
  sessionID_(0),
  controller_(controller),
  timeout_(timeout)
{
}

FlowAgent::~FlowAgent()
{
}

void FlowAgent::setCandidates(QList<std::shared_ptr<ICEPair>> *candidates)
{
  Q_ASSERT(candidates != nullptr);
  Q_ASSERT(candidates->size() != 0);

  candidates_ = candidates;
}

void FlowAgent::setSessionID(uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);

  sessionID_ = sessionID;
}

void FlowAgent::nominationDone(std::shared_ptr<ICEPair> connection)
{
  Q_ASSERT(connection != nullptr);

  nominated_mtx.lock();

  if (connection->local->component == RTP)
  {
    nominated_[connection->local->address].first = connection;
  }
  else
  {
    nominated_[connection->local->address].second = connection;
  }

  if (nominated_[connection->local->address].first  != nullptr &&
      nominated_[connection->local->address].second != nullptr)
  {
    nominated_rtp_  = nominated_[connection->local->address].first;
    nominated_rtcp_ = nominated_[connection->local->address].second;

    nominated_rtp_->state  = PAIR_NOMINATED;
    nominated_rtcp_->state = PAIR_NOMINATED;

    emit endNomination();
    nominated_mtx.unlock();
    return;
  }

  nominated_mtx.unlock();
}

bool FlowAgent::waitForEndOfNomination(unsigned long timeout)
{
  QTimer timer;
  QEventLoop loop;

  timer.setSingleShot(true);

  QObject::connect(
      this,
      &FlowAgent::endNomination,
      &loop,
      &QEventLoop::quit,
      Qt::DirectConnection
  );

  QObject::connect(
      &timer,
      &QTimer::timeout,
      &loop,
      &QEventLoop::quit,
      Qt::DirectConnection
  );

  timer.start(timeout);
  loop.exec();

  return timer.isActive();
}


void FlowAgent::run()
{
  if (candidates_ == nullptr || candidates_->size() == 0)
  {
    printDebug(DEBUG_ERROR, this,
               "Invalid candidates, unable to perform ICE candidate negotiation!");
    emit ready(nullptr, nullptr, sessionID_);
    return;
  }

  // because we can only bind to a port once (no multithreaded access), we must divide
  // the candidates into "buckets" and each ConnectionTester is responsible for testing the bucket
  //
  // Each bucket contains all candidates for every interface.
  // ConnectionTester then binds to this interface and takes care of sending the STUN Binding requests
  std::vector<std::unique_ptr<ConnectionTester>> workerThreads;
  QList<ConnectionBucket> buckets;

  int bucketNum     = -1;
  QString prevAddr  = "";
  uint16_t prevPort = 0;

  for (int i = 0; i < candidates_->size(); ++i)
  {
    if (candidates_->at(i)->local->address != prevAddr ||
        candidates_->at(i)->local->port != prevPort)
    {
      bucketNum++;
      buckets.push_back({
          .server = new UDPServer,
          .pairs  = QList<PairStunInstance>()
      });

      // because we cannot modify create new objects from child threads (in this case new socket)
      // we must initialize both UDPServer and Stun objects here so that ConnectionTester doesn't have to do
      // anything but to test the connection
      //
      // Binding might fail, if so happens no STUN objects are created for this socket
      if (buckets[bucketNum].server->bindMultiplexed(
            QHostAddress(candidates_->at(i)->local->address),
            candidates_->at(i)->local->port) == false)
      {
        delete buckets[bucketNum].server;
        buckets[bucketNum].server = nullptr;
      }
    }

    if (buckets[bucketNum].server == nullptr)
    {
      continue;
    }

    buckets[bucketNum].pairs.push_back({
        .stun = new Stun(buckets[bucketNum].server),
        .pair = candidates_->at(i)
    });

    // when the UDPServer receives a datagram from remote->address:remote->port,
    // it will send a signal containing the datagram to this Stun object
    //
    // This way multiple Stun instances can listen to same socket
    buckets[bucketNum].server->expectReplyFrom(buckets[bucketNum].pairs.back().stun,
                                               candidates_->at(i)->remote->address,
                                               candidates_->at(i)->remote->port);

    prevAddr = candidates_->at(i)->local->address;
    prevPort = candidates_->at(i)->local->port;
  }

  for (int i = 0; i < buckets.size(); ++i)
  {
    for (int k = 0; k < buckets[i].pairs.size(); ++k)
    {
      workerThreads.push_back(std::make_unique<ConnectionTester>());

      QObject::connect(
          workerThreads.back().get(),
          &ConnectionTester::testingDone,
          this,
          &FlowAgent::nominationDone,
          Qt::DirectConnection
      );

      buckets[i].pairs[k].stun->moveToThread(workerThreads.back().get());
      workerThreads.back()->setCandidatePair(buckets[i].pairs[k].pair);
      workerThreads.back()->setStun(buckets[i].pairs[k].stun);
      workerThreads.back()->isController(controller_);
      workerThreads.back()->start();
    }
  }

  bool nominationSucceeded = waitForEndOfNomination(timeout_);

  // kill all threads, regardless of whether nomination succeeded or not
  for (size_t i = 0; i < workerThreads.size(); ++i)
  {
    workerThreads[i]->quit();
    workerThreads[i]->wait();
  }

  // deallocate all memory consumed by connection buckets
  for (int i = 0; i < buckets.size(); ++i)
  {
    if (buckets[i].server)
    {
      buckets[i].server->unbind();
      delete buckets[i].server;
    }

    for (int k = 0; k < buckets[i].pairs.size(); ++k)
    {
      delete buckets[i].pairs[k].stun;
    }
  }

  // wait for nomination from remote, wait at most 20 seconds
  if (!nominationSucceeded)
  {
    printError(this, "Nomination from remote was not received in time!");
    emit ready(nullptr, nullptr, sessionID_);
    return;
  }

  if (controller_)
  {
    Stun stun;
    if (!stun.sendNominationRequest(nominated_rtp_.get()))
    {
      printDebug(DEBUG_ERROR, "FlowAgent",  "Failed to nominate RTP candidate!");
      emit ready(nullptr, nullptr, sessionID_);
      return;
    }

    if (!stun.sendNominationRequest(nominated_rtcp_.get()))
    {
      printDebug(DEBUG_ERROR, "FlowAgent",  "Failed to nominate RTCP candidate!");
      emit ready(nominated_rtp_, nullptr, sessionID_);
      return;
    }

    nominated_rtp_->state  = PAIR_NOMINATED;
    nominated_rtcp_->state = PAIR_NOMINATED;
  }

  emit ready(nominated_rtp_, nominated_rtcp_, sessionID_);
}
