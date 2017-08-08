#include "dshowcamerafilter.h"
#include "statisticsinterface.h"

#include "dshow/capture_interface.h"

#include <QSettings>
#include <QDateTime>
#include <QDebug>

DShowCameraFilter::DShowCameraFilter(QString id, StatisticsInterface *stats)
  :Filter(id, "Camera", stats, false, true),
    settingsID_(0)
{}

void DShowCameraFilter::init()
{
  dshow_initCapture();
  int8_t count;
  dshow_queryDevices(&devices, &count);

  QSettings settings;

  QString deviceName = settings.value("video/Device").toString();
  int deviceID = settings.value("video/DeviceID").toInt();

  qDebug() << "Getting camera from settings. ID:" << deviceID << "Name:" << deviceName;

  if(count == 0)
    return;

  if(deviceID < count)
  {
    // if the deviceID has changed
    if(devices[deviceID] != deviceName)
    {
      // search for device with same name
      for(int i = 0; i < count; ++i)
      {
        if(devices[i] == deviceName)
        {
          deviceID = i;
          break;
        }
      }
      // previous camera could not be found, use default.
      deviceID = 0;
    }
  }

  if (!dshow_selectDevice(deviceID))
  {
    qDebug() << "Could not select device from settings. Trying first";
    if(!dshow_selectDevice(0))
    {
      qDebug() << "Error selecting device!";
      return;
    }
  }

  dshow_getDeviceCapabilities(&list_, &count);
  if (!dshow_setDeviceCapability(settingsID_))
  {
    qDebug() << "Error selecting capability!";
    return;
  }

  qDebug() << "Found " << (int)count << " capabilities: ";
  for (int i = 0; i < count; i++) {
    qDebug()  << "[" << i << "] " << list_[i].width << "x" << list_[i].height  << " " << list_[i].fps << "fps";
  }

  stats_->videoInfo(list_[settingsID_].fps, QSize(list_[settingsID_].width, list_[settingsID_].height));

  qDebug() << "Starting DShow camera";
  dshow_startCapture();
}

void DShowCameraFilter::stop()
{
  run_ = false;
}

void DShowCameraFilter::run()
{
  Q_ASSERT(list_ != 0);

  qDebug() << "Start taking frames from DShow camera";

  run_ = true;

  while (run_) {
    // sleep half of what is between frames. TODO sleep correct amount
    _sleep(500/list_[settingsID_].fps);
    uint8_t *data;
    uint32_t size;
    while (dshow_queryFrame(&data, &size))
    {
      Data * newImage = new Data;

      // set time
      timeval present_time;
      present_time.tv_sec = QDateTime::currentMSecsSinceEpoch()/1000;
      present_time.tv_usec = (QDateTime::currentMSecsSinceEpoch()%1000) * 1000;

      newImage->presentationTime = present_time;
      newImage->type = RGB32VIDEO;
      newImage->data = std::unique_ptr<uchar[]>(data);
      data = 0;
      newImage->data_size = size;

      newImage->height = list_[settingsID_].height;
      newImage->width = list_[settingsID_].width;

      newImage->source = LOCAL;
      newImage->framerate = list_[settingsID_].fps;

      //qDebug() << "Frame generated. Width: " << newImage->width
      //         << ", height: " << newImage->height << "Framerate:" << newImage->framerate;

      std::unique_ptr<Data> u_newImage( newImage );
      sendOutput(std::move(u_newImage));
    }
  }
}
