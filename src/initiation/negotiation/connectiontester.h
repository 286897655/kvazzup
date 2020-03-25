#pragma once

#include "icetypes.h"
#include "udpserver.h"
#include "stunmessagefactory.h"

#include <QThread>
#include <memory>

class ConnectionTester : public QThread
{
  Q_OBJECT

public:
  ConnectionTester(UDPServer *server, bool multiplex);
  ~ConnectionTester();
  void setCandidatePair(std::shared_ptr<ICEPair> pair);

  // controller_ defines the course of action after candiate pair has been validated.
  // If the ConnectionTester belongs to FlowAgent it terminates immediately after
  // confirming the validity of pair and FlowAgent can start the nomination process
  //
  // If the owner is instead FlowAgent, ConnectionTester start responding to nomination
  // requests after it has concluded the candidate verification
  void isController(bool controller);

  // Send the nominated candidate to ICE_CONTROLLED agent
  bool sendNominationRequest(ICEPair *pair);

public slots:
  // Because the Stun object used by ConnectionTester has it's own event loop, we must
  // override the default quit function, call Stun::stopTesting() and then exit from ConnectionTester
  void quit();

signals:
  // testingDone() is emitted when the connection testing has ended
  //
  // if the tested candidate succeeded (remote responded to our requests),
  // connection points to valid ICEPair, otherwise it's nullptr
  void testingDone(std::shared_ptr<ICEPair> connection);

  void parsingDone();
  void nominationRecv();
  void requestRecv();
  void stopEventLoop();


protected:
  void printMessage(QString message);
  void run();

private slots:
  void recvStunMessage(QNetworkDatagram message);

private:

  // send stun binding request to remote
  // this function is used to establish a gateway between clients
  //
  // return true if we got a response from remote and false if
  // after N tries the remote was silent
  bool sendBindingRequest(ICEPair *pair, bool controller);

  // Send STUN Binding Response to remote. Each message should be response to alraedy-received
  // request and this is why the request we're responding to is given as parameter
  //
  // TransactionID from request is copied to response
  bool sendBindingResponse(STUNMessage& request, QString addressRemote, int portRemote);



  // this function is called by the ICE_CONTROLLED agent
  //
  // It is called when the two clients have negotiated a list of possible candidates.
  // The ICE_CONTROLLING agent selects RTP and RTCP candidates used for media streaming
  // and sends these two candidates using STUN Binding request
  //
  // ICE_CONTROLLED agent (caller of sendNominationResponse()) must send STUN Binding Requests
  // to all valid candidate address:port pairs to receive the nominations
  bool sendNominationResponse(ICEPair *pair);

  // waitForStunResponse() starts an even loop which listens to parsingDone() signal
  // When the signal is received the event loop is stopped and waitForStunResponse() returns true
  // If parsingDone() signal is not received in time, waitForStunResponse() returns false
  bool waitForStunResponse(unsigned long timeout);

  // Same as waitForStunResponse() but this function listens to requestRecv() signal
  bool waitForStunRequest(unsigned long timeout);

  // Same as waitForStunResponse/Request() but this waits for nominationRecv() signal
  bool waitForNominationRequest(unsigned long timeout);

  // If we're the controlling agent we start by sending STUN Binding Requests to remote
  // When we receive a response to one of our STUN Binding Requests, we start listening to
  // remote's STUN Binding Requests. When we receive one, we acknowledge it by sending STUN
  // Binding Response to remote.
  // Now the we've concluded that the communication works and we can stop testing.
  //
  // Return true if the testing succeeded, false otherwise
  bool controllerSendBindingRequest(ICEPair *pair);

  // if we're the controlled agent, we must first start sending "dummy" binding requests to
  // make a hole in the firewall and after we've gotten a request from remote (controlling agent)
  // we can switch to sending responses

  // if we're the controlled agent, we start sending STUN Binding Requests to create a hole in the
  // firewall. When we get a response from remote (the controlling agent) we acknowledge it by sending
  // STUN Binding Request.
  //
  // After we've received a STUN Request from remote, we start listening to responses for our STUN Binding Requests.
  // When we've received a response we mark this pair as valid as we've established that communication works to both directions
  //
  // Return true if the testing succeeded, false otherwise
  bool controlleeSendBindingRequest(ICEPair *pair);

  bool sendRequestWaitResponse(ICEPair *pair, QByteArray &request, int retries, int baseTimeout);


  std::shared_ptr<ICEPair> pair_;
  bool controller_;

  // TODO [Encryption] Use TLS to send packet
  UDPServer *udp_;

  StunMessageFactory stunmsg_;

  // If multiplex_ is true, it means that the UDPServer has already been created for us
  // and we shouldn't unbind/rebind it or attach listeners to it.
  bool multiplex_;

  // When waitFor(Stun|Nomination)(Request|Response) returns, the calling code should
  // check whether interrupt flag has been set. It means that the running thread has
  // decided to terminate processing and Stun shouln't continue with the process any further
  bool interrupted_;
};
