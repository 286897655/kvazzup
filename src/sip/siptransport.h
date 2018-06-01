#pragma once

#include "sip/siptypes.h"
#include "tcpconnection.h"
#include <QHostAddress>
#include <QString>

#include <memory>

class SIPTransport : public QObject
{
  Q_OBJECT
public:
  SIPTransport(quint32 transportID);
  ~SIPTransport();

  // TODO: separate non-dialog and dialog messages

  // functions for manipulating network connection
  void createConnection(ConnectionType type, QString target);
  void incomingTCPConnection(std::shared_ptr<TCPConnection> con);
  void destroyConnection();

  // sending messages
  void sendRequest(SIPRequest &request, QVariant& content);
  void sendResponse(SIPResponse &response, QVariant& content);

  QString getLocalAddress();

public slots:
  // called when connection receives a message
  void networkPackage(QString package);

  // called when a connection is established and ip known.
  // This function may be replaced by something in the future
  void connectionEstablished(QString localAddress, QString remoteAddress);

signals:
  // signal that ads transportID to connectionEstablished slot
  void sipTransportEstablished(quint32 transportID, QString localAddress, QString remoteAddress);

  // signals that output parsed sip messages
  void incomingSIPRequest(SIPRequest request, quint32 transportID, QVariant& content);
  void incomingSIPResponse(SIPResponse response, quint32 transportID, QVariant& content);

  // we got a message, but could not parse it.
  void parsingError(ResponseType errorResponse, quint32 transportID);

private:

  // composing
  bool composeMandatoryFields(QList<SIPField>& fields, std::shared_ptr<SIPMessageInfo> message);
  QString fieldsToString(QList<SIPField>& fields, QString lineEnding);
  QString addContent(QList<SIPField>& fields, bool haveContent, const SDPMessageInfo& sdp);

  // parsing
  void parsePackage(QString package, QString& header, QString& body);
  bool headerToFields(QString header, QString& firstLine, QList<SIPField>& fields);
  bool fieldsToMessage(QList<SIPField>& fields, std::shared_ptr<SIPMessageInfo> &message);
  bool parseRequest(QString requestString, QString version,
                    std::shared_ptr<SIPMessageInfo> message,
                    QList<SIPField>& fields, QVariant& content);
  bool parseResponse(QString responseString, QString version,
                     std::shared_ptr<SIPMessageInfo> message,
                     QVariant& content);

  void parseContent(QVariant &content, ContentType type, QString &body);

  void parseSIPaddress(QString address, QString& user, QString& location);
  QList<QHostAddress> parseIPAddress(QString address);

  void signalConnections();

  QString partialMessage_;

  std::shared_ptr<TCPConnection> connection_;
  quint32 transportID_;
};
