#include "audiocapturefilter.h"
#include "audiocapturedevice.h"
#include "statisticsinterface.h"

#include <QDebug>
#include <QTime>

const int AUDIO_BUFFER_SIZE = 65536;

AudioCaptureFilter::AudioCaptureFilter(QString id, QAudioFormat format, StatisticsInterface *stats) :
  Filter(id, "Audio_Capture", stats, NONE, RAWAUDIO),
  deviceInfo_(QAudioDeviceInfo::defaultInputDevice()),
  device_(NULL),
  format_(format),
  audioInput_(NULL),
  input_(NULL),
  buffer_(AUDIO_BUFFER_SIZE, 0)
{}

AudioCaptureFilter::~AudioCaptureFilter(){}

bool AudioCaptureFilter::init()
{
  qDebug() << "Initializing audio capture filter";

  QAudioDeviceInfo info(deviceInfo_);

  for(auto device : QAudioDeviceInfo::availableDevices(QAudio::AudioInput))
  {
    qDebug() << "Audio Recording device:" << device.deviceName();
  }

  qDebug() << "Using device:" << info.deviceName();

  if (!info.isFormatSupported(format_)) {
    qWarning() << "Default audio format not supported - trying to use nearest";
    format_ = info.nearestFormat(format_);
  }

  if(format_.sampleRate() != -1)
    stats_->audioInfo(format_.sampleRate(), format_.channelCount());
  else
    stats_->audioInfo(0, 0);

  if (device_)
    delete device_;
  device_  = new AudioCaptureDevice(format_, this);

  createAudioInput();
  qDebug() << "Audio initializing completed";

  return true;
}

void AudioCaptureFilter::createAudioInput()
{
  qDebug() << "Creating audio input";
  audioInput_ = new QAudioInput(deviceInfo_, format_, this);

  device_->start();

  input_ = audioInput_->start();
  connect(input_, SIGNAL(readyRead()), SLOT(readMore()));

}

void AudioCaptureFilter::readMore()
{
  if (!audioInput_)
  {
    qWarning() << "No audio input in readMore";
    return;
  }
  qint64 len = audioInput_->bytesReady();
  if (len > AUDIO_BUFFER_SIZE)
  {
    len = AUDIO_BUFFER_SIZE;
  }
  qint64 l = input_->read(buffer_.data(), len);

  if (l > 0)
  {
    device_->write(buffer_.constData(), l);

    Data* newSample = new Data;

    // create audio data packet to be sent to filter graph
    timeval present_time;
    present_time.tv_sec = QDateTime::currentMSecsSinceEpoch()/1000;
    present_time.tv_usec = (QDateTime::currentMSecsSinceEpoch()%1000) * 1000;
    newSample->presentationTime = present_time;
    newSample->type = RAWAUDIO;
    newSample->data = std::unique_ptr<uint8_t[]>(new uint8_t[len]);

    memcpy(newSample->data.get(), buffer_.constData(), len);

    newSample->data_size = len;
    newSample->width = 0;
    newSample->height = 0;
    newSample->source = LOCAL;
    newSample->framerate = format_.sampleRate();

    std::unique_ptr<Data> u_newSample( newSample );
    sendOutput(std::move(u_newSample));

    //qDebug() << "Audio capture: Generated sample with size:" << l;
  }
}

void AudioCaptureFilter::start()
{
  qDebug() << "Resuming audio input.";

  if (audioInput_->state() == QAudio::SuspendedState
      || audioInput_->state() == QAudio::StoppedState)
  {
    audioInput_->resume();
  }
}

void AudioCaptureFilter::stop()
{
  qDebug() << "Suspending audio input.";

  if (audioInput_->state() == QAudio::ActiveState)
  {
    audioInput_->suspend();
  }

  // just in case the filter part was running
  Filter::stop();

  qDebug() << "Audio input suspended.";
}

// changing of audio device mid stream.
void AudioCaptureFilter::deviceChanged(int index)
{
  qWarning() << "WARNING: audiocapturefilter device change not implemented fully:" << index;

  device_->stop();
  audioInput_->stop();
  audioInput_->disconnect(this);
  delete audioInput_;

  init();
}

void AudioCaptureFilter::volumeChanged(int value)
{
  if(audioInput_)
  {
    audioInput_->setVolume(qreal(value) / 100);
  }
}

void AudioCaptureFilter::process()
{}
