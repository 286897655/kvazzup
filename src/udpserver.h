#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QHostAddress>
#include <QNetworkDatagram>

#include <stdint.h>

class QUdpSocket;

class UDPServer : public QObject
{
  Q_OBJECT
public:
  UDPServer();

  void bind(const QHostAddress& address, quint16 port);
  bool bindRaw(const QHostAddress& address, quint16 port);

  // sends the data using Qt UDP classes.
  void sendData(QByteArray& data, const QHostAddress &address, quint16 port, bool untilReply);

signals:

  // send message data forward.
  void messageAvailable(QByteArray message);
  void rawMessageAvailable(QNetworkDatagram message);

private slots:
  // read the data when it becomes available
  void readData();

  // read the data when it becomes available
  void readRawData();

private:
  bool bindSocket(const QHostAddress &address, quint16 port, bool raw);

  QUdpSocket* socket_;

  QTimer resendTimer_;
  bool waitingReply_;

  int tryNumber_;
  uint16_t sendPort_;
};
