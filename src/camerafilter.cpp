#include "camerafilter.h"

#include "cameraframegrabber.h"

//#include <QCameraViewfinder>
#include <QSettings>
#include <QCameraInfo>
#include <QTime>
#include <QtDebug>

#include "statisticsinterface.h"


CameraFilter::CameraFilter(QString id, StatisticsInterface *stats):
  Filter(id, "Camera", stats, false, true),
  camera_(),
  cameraFrameGrabber_(),
  framerate_(0)
{
  QList<QCameraInfo> cameras = QCameraInfo::availableCameras();

  if(cameras.size() == 0)
  {
    return;
  }

  QSettings settings;
  QString deviceName = settings.value("video/Device").toString();
  int deviceID = settings.value("video/DeviceID").toInt();

  // if the deviceID has changed
  if(deviceID < cameras.size() && cameras[deviceID].deviceName() != deviceName)
  {
    // search for device with same name
    for(int i = 0; i < cameras.size(); ++i)
    {
      if(cameras.at(i).deviceName() == deviceName)
      {
        deviceID = i;
        break;
      }
    }
    // previous camera could not be found, use first.
    deviceID = 0;
  }

  camera_ = new QCamera(cameras.at(deviceID));
  //camera_ = new QCamera(QCameraInfo::defaultCamera());
  cameraFrameGrabber_ = new CameraFrameGrabber();

  Q_ASSERT(camera_ && cameraFrameGrabber_);

  camera_->setViewfinder(cameraFrameGrabber_);

  connect(cameraFrameGrabber_, SIGNAL(frameAvailable(QVideoFrame)), this, SLOT(handleFrame(QVideoFrame)));

  QCameraViewfinderSettings viewSettings = camera_->viewfinderSettings();
  camera_->supportedViewfinderResolutions();
  viewSettings.setResolution(QSize(640, 480));
  viewSettings.setPixelFormat(QVideoFrame::Format_Jpeg);
  camera_->setViewfinderSettings(viewSettings);
  camera_->start();
}

CameraFilter::~CameraFilter()
{
  delete camera_;
  delete cameraFrameGrabber_;
}

void CameraFilter::process()
{
  // do nothing because camera input is handled via camera signal
}

void CameraFilter::start()
{
  if(camera_->state() == QCamera::LoadedState)
  {
    camera_->start();
  }
}

void CameraFilter::stop()
{
  if(camera_->state() == QCamera::ActiveState)
  {
    camera_->stop();
  }
  Filter::stop();
}

void CameraFilter::handleFrame(const QVideoFrame &frame)
{
  if(framerate_ == 0)
  {
    QCameraViewfinderSettings settings = camera_->viewfinderSettings();
    QList<QVideoFrame::PixelFormat> formats = camera_->supportedViewfinderPixelFormats();
    /*
    for(auto format : formats)
    {
      qDebug() << "QCamera supported format:" << format;
    }

    QList<QSize> resolutions = camera_->supportedViewfinderResolutions();
    for(auto reso : resolutions)
    {
      qDebug() << "QCamera supported resolutions:" << reso;
    }
    */
    stats_->videoInfo(settings.maximumFrameRate(), settings.resolution());
    framerate_ = settings.maximumFrameRate();
  }

  QVideoFrame cloneFrame(frame);
  cloneFrame.map(QAbstractVideoBuffer::ReadOnly);

  QVideoFrame::PixelFormat pf = cloneFrame.pixelFormat();

  Q_ASSERT(pf == QVideoFrame::Format_RGB32);

  // capture the frame data
  Data * newImage = new Data;

  // set time
  timeval present_time;

  present_time.tv_sec = QDateTime::currentMSecsSinceEpoch()/1000;
  present_time.tv_usec = (QDateTime::currentMSecsSinceEpoch()%1000) * 1000;

  newImage->presentationTime = present_time;
  newImage->type = RGB32VIDEO;
  newImage->data = std::unique_ptr<uchar[]>(new uchar[cloneFrame.mappedBytes()]);

  uchar *bits = cloneFrame.bits();

  memcpy(newImage->data.get(), bits, cloneFrame.mappedBytes());
  newImage->data_size = cloneFrame.mappedBytes();
  // kvazaar requires divisable by 8 resolution
  newImage->width = cloneFrame.width() - cloneFrame.width()%8;
  newImage->height = cloneFrame.height() - cloneFrame.height()%8;
  newImage->source = LOCAL;
  newImage->framerate = framerate_;
/*
  // scale the image and copy to new data
  if(resolution_ != cloneFrame.size())
  {
    QImage image(
          newImage->data.get(),
          newImage->width,
          newImage->height,
          QImage::Format_RGB32);

    QImage image2 = image.scaled(resolution_);
    if(resolution_.width() * resolution_.height()
       > cloneFrame.size().width() * cloneFrame.size().height())
    {
      newImage->data = std::unique_ptr<uchar[]>(new uchar[image2.byteCount()]);
    }
    memcpy(newImage->data.get(), image2.bits(), image2.byteCount());
    newImage->width = resolution_.width();
    newImage->height = resolution_.height();
    newImage->data_size = image2.byteCount();
    newImage->framerate = 30; // TODO Input this using settings.
  }
*/
  //qDebug() << "Frame generated. Format: " << pf
  //         << " width: " << newImage->width << ", height: " << newImage->height;

  std::unique_ptr<Data> u_newImage( newImage );
  cloneFrame.unmap();

  Q_ASSERT(u_newImage->data);
  sendOutput(std::move(u_newImage));
}
