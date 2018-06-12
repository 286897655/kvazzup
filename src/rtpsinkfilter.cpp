#include "rtpsinkfilter.h"

#include <statisticsinterface.h>
#include "common.h"

#include <RTPInterface.hh>

#include <QDebug>

const uint32_t BUFFER_SIZE = 10*65536;

RTPSinkFilter::RTPSinkFilter(QString id, StatisticsInterface *stats, UsageEnvironment& env, DataType type, QString media):
  Filter(id, "RTP_Sink_" + media, stats, NONE, type),
  MediaSink(env),
  type_(type)
{
  fReceiveBuffer = new u_int8_t[BUFFER_SIZE];
  stats_->addFilterTID(name_, (uint64_t)currentThreadId());
}

RTPSinkFilter::~RTPSinkFilter()
{}

void RTPSinkFilter::uninit()
{
  stopPlaying();
  while (fSource  != NULL)
  {
    qSleep(1);
  }
  qDebug() << "Deleting RTPSink:" << name_ << "type:" << type_;
  delete fReceiveBuffer;
  fReceiveBuffer = 0;
}

void RTPSinkFilter::afterGettingFrame(void* clientData,
                              unsigned frameSize,
                              unsigned numTruncatedBytes,
                              struct timeval presentationTime,
                              unsigned durationInMicroseconds)
{
  Q_ASSERT(clientData);
  RTPSinkFilter* sink = (RTPSinkFilter*)clientData;
  sink->afterGettingFrame(frameSize, numTruncatedBytes,
                          presentationTime, durationInMicroseconds);
}

void RTPSinkFilter::afterGettingFrame(unsigned frameSize,
                       unsigned numTruncatedBytes,
                       struct timeval presentationTime,
                       unsigned durationInMicroseconds)
{
  //qDebug() << "Received HEVC frame. Size: " << frameSize
  //         << ", truncated: " << numTruncatedBytes;

  Q_UNUSED(durationInMicroseconds);

  // TODO increase buffer for large frames
  Q_ASSERT(numTruncatedBytes == 0);

  stats_->addReceivePacket(frameSize);

  Data *received_picture = new Data;
  received_picture->data_size = frameSize;
  received_picture->type = type_;
  received_picture->data = std::unique_ptr<uchar[]>(new uchar[received_picture->data_size]);
  received_picture->width = 0; // not know at this point. Decoder tells the correct resolution
  received_picture->height = 0;
  received_picture->framerate = 0;
  received_picture->presentationTime = presentationTime;
  received_picture->source = REMOTE;
  memcpy(received_picture->data.get(), fReceiveBuffer, received_picture->data_size);
  std::unique_ptr<Data> rp( received_picture );
  sendOutput(std::move(rp));

  continuePlaying();
}

void RTPSinkFilter::process()
{}

Boolean RTPSinkFilter::continuePlaying() {
  if (fSource == NULL) return False; // sanity check (should not happen)

  // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
  fSource->getNextFrame(fReceiveBuffer, BUFFER_SIZE,
                        afterGettingFrame, this,
                        onSourceClosure, this);
  return True;
}
