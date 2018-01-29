#pragma once


#include "siptypes.h"
#include <connection.h>

#include <QHostAddress>
#include <QString>

#include <memory>

enum ConnectionType {ANY, TCP, UDP, TSL};

class SIPConnection : public QObject
{
  SIPConnection();
  ~SIPConnection();

  void initConnection(ConnectionType type, QHostAddress target);

  void sendRequest(RequestType request,
                   std::shared_ptr<SIPRoutingInfo> routing,
                   std::shared_ptr<SIPSessionInfo> session,
                   std::shared_ptr<SIPMessageInfo> message);

  void sendResponse(ResponseType response,
                    std::shared_ptr<SIPRoutingInfo> routing,
                    std::shared_ptr<SIPSessionInfo> session,
                    std::shared_ptr<SIPMessageInfo> message);

public slots:

  void networkPackage(QString message);

signals:

  void incomingSIPRequest(RequestType request,
                          std::shared_ptr<SIPRoutingInfo> routing,
                          std::shared_ptr<SIPSessionInfo> session,
                          std::shared_ptr<SIPMessageInfo> message,
                          quint32 sessionID_);

  void incomingSIPResponse(ResponseType response,
                           std::shared_ptr<SIPRoutingInfo> routing,
                           std::shared_ptr<SIPSessionInfo> session,
                           std::shared_ptr<SIPMessageInfo> message,
                           quint32 sessionID_);

private:

  struct SIPParameter
  {
    QString name;
    QString value;
  };

  struct SIPField
  {
    QString name;
    QList<QString>* values;
    QList<SIPParameter>* parameters;
  };

  void parsePackage(QString package, QString& header, QString& field);
  std::shared_ptr<QList<SIPField>> networkToFields(QString package);
  bool checkFields(std::shared_ptr<QList<SIPField>> fields);
  void fieldsToStructs(std::shared_ptr<QList<SIPField>> fields,
                       std::shared_ptr<SIPRoutingInfo> outRouting,
                       std::shared_ptr<SIPSessionInfo> outSession,
                       std::shared_ptr<SIPMessageInfo> outMessage);

  std::shared_ptr<SDPMessageInfo> parseSDPMessage(QString& body);

  void parseSIPaddress(QString address, QString& user, QString& location);
  void parseSIPParameter(QString field, QString parameterName,
                         QString& parameterValue, QString& remaining);
  QList<QHostAddress> parseIPAddress(QString address);

  bool checkSDPLine(QStringList& line, uint8_t expectedLength, QString& firstValue);

  QString partialMessage_;

  Connection connection_;

  quint32 sessionID_;
};

std::shared_ptr<SDPMessageInfo> parseSDPMessage(QString& body);

QList<QHostAddress> parseIPAddress(QString address);





