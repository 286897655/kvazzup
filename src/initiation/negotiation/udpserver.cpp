#include "common.h"
#include "udpserver.h"
#include <QUdpSocket>
#include <QMetaObject>
#include "stun.h"

UDPServer::UDPServer():
  socket_(nullptr),
  sendPort_(0)
{}

bool UDPServer::bindSocket(const QHostAddress& address, quint16 port, enum SOCKET_TYPE type)
{
  this->unbind();

  sendPort_ = port;
  socket_ = new QUdpSocket(this);

  if(!socket_->bind(address, port))
  {
    printDebug(DEBUG_ERROR, "UDPServer", DC_NEGOTIATING,
        "Failed to bind UDP Socket to", { address.toString() });
    return false;
  }

  switch (type)
  {
    case SOCKET_NORMAL:
      connect(socket_, &QUdpSocket::readyRead, this, &UDPServer::readData);
      break;

    case SOCKET_RAW:
      connect(socket_, &QUdpSocket::readyRead, this, &UDPServer::readRawData);
      break;

    case SOCKET_MULTIPLEXED:
      connect(socket_, &QUdpSocket::readyRead, this, &UDPServer::readMultiplexData);
      break;
  }

  return true;
}

void UDPServer::unbind()
{
  sendPort_ = 0;

  if (socket_)
  {
    socket_->blockSignals(true);
    socket_->close();
    delete socket_;
    socket_ = nullptr;
  }
}

void UDPServer::bind(const QHostAddress &address, quint16 port)
{
  (void)bindSocket(address, port, SOCKET_NORMAL);
}

bool UDPServer::bindRaw(const QHostAddress &address, quint16 port)
{
  return bindSocket(address, port, SOCKET_RAW);
}

bool UDPServer::bindMultiplexed(const QHostAddress& address, quint16 port)
{
  return bindSocket(address, port, SOCKET_MULTIPLEXED);
}

void UDPServer::sendData(
    QByteArray& data,
    const QHostAddress& address,
    quint16 port,
    bool untilReply
)
{
  /* qDebug() << "Sending the following UDP data:" << QString::fromStdString(data.toHex().toStdString()) */
  /*          << "with size:" << data.size(); */

  if(data.size() > 512)
  {
    printDebug(DEBUG_WARNING, "UDPServer",
               DC_NEGOTIATING, "Sending too large UDP packet!");
   // TODO do something maybe
  }
  else if (data.size() == 0)
  {
    printDebug(DEBUG_WARNING, "UDPServer",
               DC_NEGOTIATING, "Trying to send an empty UDP packet!");
    return;
  }

  QNetworkDatagram datagram = QNetworkDatagram(data, address, port);
  datagram.setSender(QHostAddress::AnyIPv4, (quint16)sendPort_);

  if (socket_->writeDatagram(datagram) < 0)
  {
    //printDebug(DEBUG_ERROR, "UDPServer",
    //           DC_NEGOTIATING, "Failed to send UDP datagram!");
  }
}

void UDPServer::readData()
{
  while (socket_->hasPendingDatagrams())
  {
    QNetworkDatagram datagram = socket_->receiveDatagram();
    emit messageAvailable(datagram.data());
  }
}

void UDPServer::readRawData()
{
  while (socket_->hasPendingDatagrams())
  {
    emit rawMessageAvailable(socket_->receiveDatagram());
  }
}

void UDPServer::readMultiplexData()
{
  while (socket_->hasPendingDatagrams())
  {
    QNetworkDatagram datagram = socket_->receiveDatagram();

    // is anyone listening to  messages from this sender?
    if (listeners_.contains(datagram.senderAddress().toString()))
    {
      if (listeners_[datagram.senderAddress().toString()]
          .contains(datagram.senderPort()))
      {
        QMetaObject::invokeMethod(
            listeners_[datagram.senderAddress().toString()][datagram.senderPort()],
            "recvStunMessage",
            Qt::DirectConnection,
            Q_ARG(QNetworkDatagram, datagram)
        );
      }
    }
  }
}

void UDPServer::expectReplyFrom(Stun *stun, QString& address, quint16 port)
{
    listeners_[address][port] = stun;
}
