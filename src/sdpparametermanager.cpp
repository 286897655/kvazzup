#include "sdpparametermanager.h"

#include <QUdpSocket>
#include <QDebug>

const uint16_t MIN_SIP_PORT   = 21500;
const uint16_t MAX_SIP_PORT   = 22000;
const uint16_t ICE_PORT_START = 21900;

const uint16_t MAX_PORTS = 42;

SDPParameterManager::SDPParameterManager()
  :remainingPorts_(0)
{
  setPortRange(MIN_SIP_PORT, MAX_SIP_PORT, MAX_PORTS);
}


void SDPParameterManager::setPortRange(uint16_t minport, uint16_t maxport, uint16_t maxRTPConnections)
{
  if(minport >= maxport)
  {
    qDebug() << "ERROR: Min port is smaller or equal to max port for SDP";
  }

  for(uint16_t i = minport; i < maxport; i += 2)
  {
    makePortPairAvailable(i);
  }

  remainingPorts_ = maxRTPConnections;
}



// for reference on rtp payload type numbers:
// https://en.wikipedia.org/wiki/RTP_audio_video_profile

QList<uint8_t> SDPParameterManager::audioPayloadTypes() const
{
  // TODO: payload type 0 should always be supported!
  return QList<uint8_t>{};
}

QList<uint8_t> SDPParameterManager::videoPayloadTypes() const
{
  return QList<uint8_t>{};
}

QList<RTPMap> SDPParameterManager::audioCodecs() const
{
  // currently we just use a different dynamic rtp payload number for each codec
  // to make implementation simpler ( range 96...127 )

  // opus is always 48000, even if the actual sample rate is lower
  return QList<RTPMap>{RTPMap{96, 48000, "opus", ""}}; // TODO: put number of channels in parameters.
}

QList<RTPMap> SDPParameterManager::videoCodecs() const
{
  return QList<RTPMap>{RTPMap{97, 90000, "h265", ""}};
}

QString SDPParameterManager::sessionName() const
{
  return "HEVC Video Call";
}

QString SDPParameterManager::sessionDescription() const
{
  return "A Kvazzup initiated video call";
}

uint16_t SDPParameterManager::nextAvailablePortPair()
{
  // TODO: I'm suspecting this may sometimes hang Kvazzup at the start

  uint16_t newLowerPort = 0;

  portLock_.lock();
  if(availablePorts_.size() >= 1 && remainingPorts_ >= 2)
  {
    QUdpSocket test_port1;
    QUdpSocket test_port2;
    do
    {
      newLowerPort = availablePorts_.at(0);
      availablePorts_.pop_front();
      remainingPorts_ -= 2;
      qDebug() << "Trying to bind ports:" << newLowerPort << "and" << newLowerPort + 1;

    } while(!test_port1.bind(newLowerPort) && !test_port2.bind(newLowerPort + 1));
    test_port1.abort();
    test_port2.abort();
  }
  else
  {
    qDebug() << "Could not reserve ports. Remaining ports:" << remainingPorts_
             << "deque size:" << availablePorts_.size();
  }
  portLock_.unlock();

  return newLowerPort;
}

void SDPParameterManager::makePortPairAvailable(uint16_t lowerPort)
{
  if(lowerPort != 0)
  {
    //qDebug() << "Freed ports:" << lowerPort << "/" << lowerPort + 1
    //         << "Ports available:" << remainingPorts_;
    portLock_.lock();
    availablePorts_.push_back(lowerPort);
    remainingPorts_ += 2;
    portLock_.unlock();
  }
}

