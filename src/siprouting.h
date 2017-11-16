#pragma once

#include <QString>
#include <QList>

#include <memory>

struct SIPRoutingInfo
{
  //what about request vs response?

  QString senderUsername;
  QString senderRealname;
  QString senderHost;
  QList<QString> senderReplyAddress;   // from via-fields. Send responses here by copying these.
  QString contactAddress;  // from contact field. Send requests here to bypass server

  QString receiverUsername;
  QString receiverRealname;
  QString receiverHost;

  QString sessionHost;
};

// This class handles routing information for one SIP dialog.
// Use this class to get necessary info for via, to, from, contact etc fields in SIP message

class SIPRouting
{
public:
  SIPRouting();

  // for peer-to-peer calls, use ip addresses as both host
  void initLocal(QString localUsername, QString sessionHost, QString localAddress, QString localHost);

  // use this if sending the first request. Not needed if the first request is sent by remote.
  void initRemote(QString remoteUsername, QString remoteHost);

  // returns whether the incoming info is valid.
  // Remember to process request in order of arrival
  // Does not check if we are allowed to receive requests from this user.
  bool incomingSIPRequest(std::shared_ptr<SIPRoutingInfo> routing);
  bool incomingSIPResponse(std::shared_ptr<SIPRoutingInfo> routing);

  // returns values to be used in SIP request. Sets directaddress if we have it.
  std::shared_ptr<SIPRoutingInfo> requestRouting(QString& directAddress);

  // copies the fields from previous request
  // sets direct contact as per RFC.
  std::shared_ptr<SIPRoutingInfo> responseRouting();

private:

  QString localUsername_;      //our username on sip server
  QString localHost_;          // name of our sip server
  QString localDirectAddress_; // to contact and via fields

  QString remoteUsername_;
  QString remoteHost_;          // name of their sip server or their ip address
  QString remoteDirectAddress_; // from contact field. Send requests here if not empty.

  QString sessionHost_;

  std::shared_ptr<SIPRoutingInfo> previousReceivedRequest_;
  std::shared_ptr<SIPRoutingInfo> previousSentRequest_;
};
