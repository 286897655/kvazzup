#include "audiocapturefilter.h"

#include <QDebug>

#include <QTime>

const int AUDIO_BUFFER_SIZE = 4096;

AudioCaptureFilter::AudioCaptureFilter(StatisticsInterface *stats) :
  Filter("Audio capture", stats, false, true),
  deviceInfo_(QAudioDeviceInfo::defaultInputDevice()),
  device_(0),
  audioInput_(0),
  input_(0),
  buffer_(AUDIO_BUFFER_SIZE, 0)
{}

AudioCaptureFilter::~AudioCaptureFilter(){}

void AudioCaptureFilter::init()
{
  initializeAudio();
}

void AudioCaptureFilter::initializeAudio()
{
  qDebug() << "Initializing audio capture filter";
  format_.setSampleRate(48000);
  format_.setChannelCount(1);
  format_.setSampleSize(16);
  format_.setSampleType(QAudioFormat::SignedInt);
  format_.setByteOrder(QAudioFormat::LittleEndian);
  format_.setCodec("audio/pcm");

  QAudioDeviceInfo info(deviceInfo_);
  if (!info.isFormatSupported(format_)) {
    qWarning() << "Default format not supported - trying to use nearest";
    format_ = info.nearestFormat(format_);
  }

  if (device_)
    delete device_;
  device_  = new AudioCaptureDevice(format_, this);
  // connect(device_, SIGNAL(update()), SLOT(refreshDisplay()));

  createAudioInput();
  qDebug() << "Audio initializing completed";
}

void AudioCaptureFilter::createAudioInput()
{
  qDebug() << "Creating audio input";
  audioInput_ = new QAudioInput(deviceInfo_, format_, this);
  //volumeSlider->setValue(audioInput->volume() * 100);

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
    len = AUDIO_BUFFER_SIZE;
  qint64 l = input_->read(buffer_.data(), len);
  if (l > 0)
    device_->write(buffer_.constData(), l);

  Data * newSample = new Data;

  // set time
  timeval present_time;
  present_time.tv_sec = QDateTime::currentMSecsSinceEpoch()/1000;
  present_time.tv_usec = (QDateTime::currentMSecsSinceEpoch()%1000) * 1000;
  newSample->presentationTime = present_time;
  newSample->type = RAWAUDIO;
  newSample->data = std::unique_ptr<uchar[]>(new uchar[len]);
  newSample->data_size = len;
  newSample->width = 0;
  newSample->height = 0;

  std::unique_ptr<Data> u_newSample( newSample );
  sendOutput(std::move(u_newSample));

  qDebug() << "Audio sample generated with size:" << len;
}


void AudioCaptureFilter::toggleSuspend()
{
  // toggle suspend/resume
  if (audioInput_->state() == QAudio::SuspendedState) {
    audioInput_->resume();
    //suspendResumeButton_->setText(tr(SUSPEND_LABEL));
  } else if (audioInput_->state() == QAudio::ActiveState) {
    audioInput_->suspend();
    //m_suspendResumeButton->setText(tr(RESUME_LABEL));
  } else if (audioInput_->state() == QAudio::StoppedState) {
    audioInput_->resume();
    //m_suspendResumeButton->setText(tr(SUSPEND_LABEL));
  } else if (audioInput_->state() == QAudio::IdleState) {
    // no-op
  }
}

// changing of audio device mid stream.
void AudioCaptureFilter::deviceChanged(int index)
{
  device_->stop();
  audioInput_->stop();
  audioInput_->disconnect(this);
  delete audioInput_;

  //deviceInfo_ = m_deviceBox->itemData(index).value<QAudioDeviceInfo>();
  initializeAudio();
}

void AudioCaptureFilter::volumeChanged(int value)
{
  if(audioInput_)
  {
    audioInput_->setVolume(qreal(value) / 100);
  }
}

void AudioCaptureFilter::process()
{

}
