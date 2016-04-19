#include "cameraframegrabber.h"

CameraFrameGrabber::CameraFrameGrabber(QObject *parent) :
  QAbstractVideoSurface(parent)
{

}

bool CameraFrameGrabber::present(const QVideoFrame &frame)
{
  if (frame.isValid()) {
    emit frameAvailable(frame);
    return true;
  }
  return false;
}

QList<QVideoFrame::PixelFormat> CameraFrameGrabber::supportedPixelFormats(QAbstractVideoBuffer::HandleType handleType) const
{
  Q_UNUSED(handleType);
  return QList<QVideoFrame::PixelFormat>()
      << QVideoFrame::Format_RGB32;
}
