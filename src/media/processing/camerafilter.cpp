#include "camerafilter.h"

#include "cameraframegrabber.h"
#include "statisticsinterface.h"

#include "common.h"

#include <QSettings>
#include <QCameraInfo>
#include <QTime>
#include <QtDebug>


CameraFilter::CameraFilter(QString id, StatisticsInterface *stats):
  Filter(id, "Camera", stats, NONE, RGB32VIDEO),
  camera_(nullptr),
  cameraFrameGrabber_(nullptr),
  framerate_(0),
  currentDeviceName_(""),
  currentDeviceID_(-1),
  currentInputFormat_(""),
  currentResolutionID_(-1),
  currentFramerateID_(-1)
{}


CameraFilter::~CameraFilter()
{
  if (camera_ != nullptr)
  {
    delete camera_;
  }
  if(cameraFrameGrabber_ != nullptr)
  {
    delete cameraFrameGrabber_;
  }
}

void CameraFilter::updateSettings()
{
  QSettings settings("kvazzup.ini", QSettings::IniFormat);

  QString deviceName = settings.value("video/Device").toString();
  int deviceID = settings.value("video/DeviceID").toInt();
  QString inputFormat = settings.value("video/InputFormat").toString();
  int resolutionID = settings.value("video/ResolutionID").toInt();
  int framerateID = settings.value("video/FramerateID").toInt();

  if (deviceName != currentDeviceName_ ||
      deviceID != currentDeviceID_ ||
      inputFormat != currentInputFormat_ ||
      resolutionID != currentResolutionID_ ||
      framerateID != currentFramerateID_)
  {
    printNormal(this, "Updating camera settings since they have changed");

    stop();
    uninit();
    init();
    start();
  }

  Filter::updateSettings();
}


void CameraFilter::uninit()
{
  stop();
  if (camera_ != nullptr)
  {
    delete camera_;
    camera_ = nullptr;
  }

  if (cameraFrameGrabber_ != nullptr)
  {
    delete cameraFrameGrabber_;
    cameraFrameGrabber_ = nullptr;
  }
}


bool CameraFilter::init()
{
  if (initialCameraSetup() && cameraSetup())
  {
    return Filter::init();
  }
  return false;
}


void CameraFilter::run()
{
  Filter::run();
}

bool CameraFilter::initialCameraSetup()
{
  QList<QCameraInfo> cameras = QCameraInfo::availableCameras();

  if(cameras.size() == 0)
  {
    qDebug() << "No camera found!";
    return false;
  }

  QSettings settings("kvazzup.ini", QSettings::IniFormat);
  currentDeviceName_ = settings.value("video/Device").toString();
  currentDeviceID_ = settings.value("video/DeviceID").toInt();

  // if the deviceID has changed
  if (currentDeviceID_ < cameras.size() && cameras[currentDeviceID_].description() != currentDeviceName_)
  {
    // search for device with same name
    for(int i = 0; i < cameras.size(); ++i)
    {
      if(cameras.at(i).description() == currentDeviceName_)
      {
        qDebug() << "Found camera with name:" << cameras.at(i).description() << "and id:" << i;
        currentDeviceID_ = i;
        break;
      }
    }
    // previous camera could not be found, use first.
    qDebug() << "Did not find camera name:" << currentDeviceName_
             << " Using first:" << cameras.at(0).description();
    currentDeviceID_ = 0;
  }

  qDebug() << "Iniating Qt camera with device ID:" << currentDeviceID_ << " cameras:" << cameras.size();

  camera_ = new QCamera(cameras.at(currentDeviceID_));
  cameraFrameGrabber_ = new CameraFrameGrabber();

#ifndef __linux__
  camera_->load();
#endif
  return true;
}


bool CameraFilter::cameraSetup()
{
  Q_ASSERT(camera_ && cameraFrameGrabber_);

  if (camera_ && cameraFrameGrabber_)
  {
    QSettings settings("kvazzup.ini", QSettings::IniFormat);
#ifndef __linux__

    int waitRoundMS = 5;
    int totalWaitTime = 1000;
    int currentWaitTime = 0;
    while(camera_->state() != QCamera::LoadedState)
    {
      qSleep(waitRoundMS);
      currentWaitTime += waitRoundMS;
      if (currentWaitTime >= totalWaitTime)
      {
        printWarning(this, "Loading camera did not succeed.");
        return false;
      }
    }
#endif

    camera_->setViewfinder(cameraFrameGrabber_);

    connect(cameraFrameGrabber_, SIGNAL(frameAvailable(QVideoFrame)), this, SLOT(handleFrame(QVideoFrame)));

    QCameraViewfinderSettings viewSettings = camera_->viewfinderSettings();
    printSupportedFormats();

    // TODO: this should be a temporary hack until dshow is replaced by qcamera
    currentInputFormat_ = settings.value("video/InputFormat").toString();

#ifdef __linux__
    viewSettings.setPixelFormat(QVideoFrame::Format_RGB32);
    output_ = RGB32VIDEO;
#else
    if(currentInputFormat_ == "MJPG")
    {
      viewSettings.setPixelFormat(QVideoFrame::Format_Jpeg);
      output_ = RGB32VIDEO;
    }
    else if(currentInputFormat_ == "RGB32")
    {
      viewSettings.setPixelFormat(QVideoFrame::Format_RGB32);
      output_ = RGB32VIDEO;
    }
    else if(currentInputFormat_ == "YUV420P")
    {
      viewSettings.setPixelFormat(QVideoFrame::Format_YUV420P);
      output_ = YUV420VIDEO;
    }
    else
    {
      printError(this, "Input format not supported");
      viewSettings.setPixelFormat(QVideoFrame::Format_Invalid);
      output_ = NONE;
      return false;
    }
    //printSupportedResolutions(viewSettings);
#endif

#ifndef __linux__
    QList<QSize> resolutions = camera_->supportedViewfinderResolutions(viewSettings);

    currentResolutionID_ = settings.value("video/ResolutionID").toInt();
    if(resolutions.size() >= currentResolutionID_ && !resolutions.empty())
    {
      QSize resolution = resolutions.at(currentResolutionID_);
      viewSettings.setResolution(QSize(resolution.width(),
                                       resolution.height()));
    }
    else {
      viewSettings.setResolution(QSize(0,0));
    }
#else
    viewSettings.setResolution(QSize(640, 480));
#endif

    QList<QCamera::FrameRateRange> framerates = camera_->supportedViewfinderFrameRateRanges(viewSettings);

    currentFramerateID_ = settings.value("video/FramerateID").toInt();

#ifndef __linux__
    if (!framerates.empty())
    {
      if(currentFramerateID_ < framerates.size())
      {
        viewSettings.setMaximumFrameRate(framerates.at(currentFramerateID_).maximumFrameRate);
        viewSettings.setMinimumFrameRate(framerates.at(currentFramerateID_).minimumFrameRate);
      }
      else {
        viewSettings.setMaximumFrameRate(framerates.back().maximumFrameRate);
        viewSettings.setMinimumFrameRate(framerates.back().minimumFrameRate);
      }
    }
#else
    viewSettings.setMaximumFrameRate(30);
    viewSettings.setMinimumFrameRate(30);
#endif

    qDebug() << "Iniating, CameraFilter : Using following QCamera settings:";
    qDebug() << "Iniating, CameraFilter :---------------------------------------";
    qDebug() << "Iniating, CameraFilter : Format:" << viewSettings.pixelFormat();
    qDebug() << "Iniating, CameraFilter : Resolution:" << viewSettings.resolution();
    qDebug() << "Iniating, CameraFilter : FrameRate:" << viewSettings.minimumFrameRate()
             << "to" << viewSettings.maximumFrameRate();
    qDebug() << "Iniating, CameraFilter : ---------------------------------------";
    camera_->setViewfinderSettings(viewSettings);
    camera_->start();

    qDebug() << "Iniating, CameraFilter : Camera status:" << camera_->state();
  }
  else
  {
    return false;
  }
  return true;
}


void CameraFilter::start()
{
  Filter::start();
  if(camera_)
  {
    camera_->start();
  }
}


void CameraFilter::stop()
{
  if(camera_)
  {
    camera_->stop();
  }
  Filter::stop();
}


void CameraFilter::handleFrame(const QVideoFrame &frame)
{
  frameMutex_.lock();
  frames_.push_back(frame);
  frameMutex_.unlock();
  wakeUp();
}


void CameraFilter::process()
{
  if(frames_.empty())
  {
    qDebug() << "No frame";
    return;
  }

  while(!frames_.empty())
  {
    QVideoFrame frame = frames_.front();
    frameMutex_.lock();
    frames_.pop_front();
    frameMutex_.unlock();

    if(framerate_ == 0)
    {
      QCameraViewfinderSettings settings = camera_->viewfinderSettings();

      getStats()->videoInfo(settings.maximumFrameRate(), settings.resolution());
      framerate_ = settings.maximumFrameRate();
    }

    // capture the frame data
    Data * newImage = new Data;
    newImage->presentationTime = QDateTime::currentMSecsSinceEpoch();
    newImage->type = output_;

    QVideoFrame cloneFrame(frame);
    cloneFrame.map(QAbstractVideoBuffer::ReadOnly);

    newImage->data = std::unique_ptr<uchar[]>(new uchar[cloneFrame.mappedBytes()]);
    uchar *bits = cloneFrame.bits();

    memcpy(newImage->data.get(), bits, cloneFrame.mappedBytes());
    newImage->data_size = cloneFrame.mappedBytes();
    // kvazaar requires divisable by 8 resolution
    newImage->width = cloneFrame.width() - cloneFrame.width()%8;
    newImage->height = cloneFrame.height() - cloneFrame.height()%8;
    newImage->source = LOCAL;
    newImage->framerate = framerate_;

    std::unique_ptr<Data> u_newImage( newImage );
    cloneFrame.unmap();

    Q_ASSERT(u_newImage->data);
    sendOutput(std::move(u_newImage));
  }
}


void CameraFilter::printSupportedFormats()
{
  QList<QVideoFrame::PixelFormat> formats = camera_->supportedViewfinderPixelFormats();
  qDebug() << "Iniating, Camerafilter: QCamera supported formats:" << formats;
}


void CameraFilter::printSupportedResolutions(QCameraViewfinderSettings& viewsettings)
{
  QList<QSize> resolutions = camera_->supportedViewfinderResolutions(viewsettings);
  qDebug() << "Iniating, CameraFilter: Found" << resolutions.size() << "supported QCamera resolutions.";
  for(auto& reso : resolutions)
  {
    qDebug() << "Iniating, CameraFilter: QCamera supported resolutions:" << reso;
  }
}
