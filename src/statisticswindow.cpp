#include "statisticswindow.h"
#include "ui_statisticswindow.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QDebug>

const int BUFFERSIZE = 65536;


StatisticsWindow::StatisticsWindow(QWidget *parent) :
QDialog(parent),
StatisticsInterface(),
ui_(new Ui::StatisticsWindow),
  framerate_(0),
  videoIndex_(0), // ringbuffer index
  videoPackets_(BUFFERSIZE,0), // ringbuffer
  audioIndex_(0), // ringbuffer index
  audioPackets_(BUFFERSIZE,0), // ringbuffer
  sendPacketCount_(0),
  transferredData_(0),
  receivePacketCount_(0),
  receivedData_(0),
  packetsDropped_(0),
  lastVideoBitrate_(0),
  lastAudioBitrate_(0)
{
  ui_->setupUi(this);
  ui_->participantTable->setColumnCount(4); // more columns can be added later
  ui_->participantTable->setHorizontalHeaderItem(0, new QTableWidgetItem(QString("IP")));
  ui_->participantTable->setHorizontalHeaderItem(1, new QTableWidgetItem(QString("Port")));
  ui_->participantTable->setHorizontalHeaderItem(2, new QTableWidgetItem(QString("Audio delay")));
  ui_->participantTable->setHorizontalHeaderItem(3, new QTableWidgetItem(QString("Video delay")));
}

StatisticsWindow::~StatisticsWindow()
{
  delete ui_;
}

void StatisticsWindow::closeEvent(QCloseEvent *event)
{
  accept();
}

void StatisticsWindow::addNextInterface(StatisticsInterface* next)
{}

void StatisticsWindow::videoInfo(double framerate, QSize resolution)
{
  framerate_ = framerate;
  ui_->framerate_value->setText( QString::number(framerate)+" fps");
  ui_->resolution_value->setText( QString::number(resolution.width()) + "x"
                          + QString::number(resolution.height()));
}

void StatisticsWindow::audioInfo(uint32_t sampleRate, uint16_t channelCount)
{
  //ui_->a_framerate_value->setText(QString::number(framerate)+"fps");
  ui_->channels_value->setText(QString::number(channelCount));
  ui_->sample_rate_value->setText(QString::number(sampleRate) + " Hz");
}

void StatisticsWindow::addParticipant(QString ip, QString port)
{
  ui_->participantTable->insertRow(ui_->participantTable->rowCount());
  // add cells to table
  ui_->participantTable->setItem(ui_->participantTable->rowCount() -1, 0, new QTableWidgetItem(ip));
  ui_->participantTable->setItem(ui_->participantTable->rowCount() -1, 1, new QTableWidgetItem(port));
  ui_->participantTable->setItem(ui_->participantTable->rowCount() -1, 2, new QTableWidgetItem("- ms"));
  ui_->participantTable->setItem(ui_->participantTable->rowCount() -1, 3, new QTableWidgetItem("- ms"));

  delays_.push_back({0,0});

}

void StatisticsWindow::sendDelay(QString type, uint32_t delay)
{
  if(type == "video" || type == "Video")
  {
    sendMutex_.lock();
    ui_->encode_delay_value->setText( QString::number(delay) + " ms. (not precise)" );
    sendMutex_.unlock();
  }
  else if(type == "audio" || type == "Audio")
  {
    sendMutex_.lock();
    ui_->audio_delay_value->setText( QString::number(delay) + " ms. (not precise)" );
    sendMutex_.unlock();
  }
}

void StatisticsWindow::receiveDelay(uint32_t peer, QString type, int32_t delay)
{
  if(type == "video" || type == "Video")
  {
    delays_.at(peer).video = delay;
    //ui_->participantTable->setItem(peer, 3, new QTableWidgetItem( QString::number(delay) + " ms"));
  }
  else if(type == "audio" || type == "Audio")
  {
    delays_.at(peer).audio = delay;
    //ui_->participantTable->setItem(peer, 2, new QTableWidgetItem( QString::number(delay) + " ms"));
  }
}

void StatisticsWindow::addEncodedPacket(QString type, uint16_t size)
{
  PacketInfo *packet = new PacketInfo{QDateTime::currentMSecsSinceEpoch(), size};

  if(type == "video" || type == "Video")
  {
    if(videoPackets_[videoIndex_%BUFFERSIZE])
    {
      delete videoPackets_.at(videoIndex_%BUFFERSIZE);
    }

    videoPackets_[videoIndex_%BUFFERSIZE] = packet;

    ++videoIndex_;
  }
  else if(type == "audio" || type == "Audio")
  {
    if(audioPackets_[audioIndex_%BUFFERSIZE])
    {
      delete audioPackets_.at(audioIndex_%BUFFERSIZE);
    }

    audioPackets_[audioIndex_%BUFFERSIZE] = packet;

    ++audioIndex_;
  }
}

uint32_t StatisticsWindow::bitrate(std::vector<PacketInfo*>& packets, uint32_t index)
{
  if(index == 0)
    return 0;

  uint32_t currentTime = QDateTime::currentMSecsSinceEpoch();
  uint32_t timeInterval = 0;
  uint32_t bitrate = 0;
  uint32_t bitrateInterval = 2000;

  uint32_t i = index - 1;

  PacketInfo* p = packets[i%BUFFERSIZE];

  while(p && timeInterval < bitrateInterval)
  {
    bitrate += p->size;
    if(i != 0)
      --i;
    else
      i = BUFFERSIZE - 1;

    timeInterval = currentTime - p->timestamp;
    p = packets[i%BUFFERSIZE];
  }

  //qDebug() << "Bitrate:" << bitrate << "timeInterval:"  << timeInterval;
  if(timeInterval)
    return 8*bitrate/(timeInterval);

  return 0;
}

void StatisticsWindow::addSendPacket(uint16_t size)
{
  sendMutex_.lock();
  ++sendPacketCount_;
  transferredData_ += size;
  sendMutex_.unlock();
}

void StatisticsWindow::addReceivePacket(uint16_t size)
{
  receiveMutex_.lock();
  ++receivePacketCount_;
  receivedData_ += size;
  receiveMutex_.unlock();
}

void StatisticsWindow::updateBufferStatus(QString filter, uint16_t buffersize)
{
  bufferMutex_.lock();
  if(buffers_.find(filter) == buffers_.end() || buffers_[filter] != buffersize)
  {
    dirtyBuffers_ = true;
    buffers_[filter] = buffersize;
  }
  bufferMutex_.unlock();
}

void StatisticsWindow::packetDropped()
{
  ++packetsDropped_;
}

void StatisticsWindow::paintEvent(QPaintEvent *event)
{
  if(videoIndex_%15 == 0)
  {
    lastVideoBitrate_ = bitrate(videoPackets_, videoIndex_);
    ui_->video_bitrate_value->setText
      ( QString::number(lastVideoBitrate_) + " kbit/s" );

    uint index = 0;
    for(Delays d : delays_)
    {
      ui_->participantTable->setItem(index, 2, new QTableWidgetItem( QString::number(d.audio) + " ms"));
      ui_->participantTable->setItem(index, 3, new QTableWidgetItem( QString::number(d.video) + " ms"));
      ++index;
    }
  }

  if(audioIndex_%20 == 0)
  {
    lastAudioBitrate_ = bitrate(audioPackets_, audioIndex_);
    ui_->audio_bitrate_value->setText
      ( QString::number(lastAudioBitrate_) + " kbit/s" );
  }

  ui_->packets_sent_value->setText( QString::number(sendPacketCount_));
  ui_->data_sent_value->setText( QString::number(transferredData_));
  ui_->packets_received_value->setText( QString::number(receivePacketCount_));
  ui_->data_received_value->setText( QString::number(receivedData_));
  ui_->packets_skipped_value->setText(QString::number(packetsDropped_));

  if(dirtyBuffers_)
  {
    bufferMutex_.lock();
    ui_->buffer_sizes_value->setText( QString::number(totalBuffers()));
    dirtyBuffers_ = false;
    bufferMutex_.unlock();
  }
}

uint32_t StatisticsWindow::totalBuffers()
{
  uint32_t total = 0;
  for(auto& it : buffers_)
  {
    total += it.second;
  }
  return total;
}
