#include "videowidget.h"

#include <QPaintEvent>
#include <QDebug>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>

unsigned int VideoWidget::number_ = 0;

VideoWidget::VideoWidget(QWidget* parent, uint8_t borderSize): QFrame(parent),
  hasImage_(false),
  previousSize_(QSize(0,0)),
  updated_(false),
  id_(number_),
  borderSize_(borderSize)
{
  ++number_;
  setAutoFillBackground(false);
  setAttribute(Qt::WA_NoSystemBackground, true);

  QPalette palette = this->palette();
  palette.setColor(QPalette::Background, Qt::black);
  setPalette(palette);

  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

  QFrame::setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  QFrame::setLineWidth(borderSize_);
  QFrame::setMidLineWidth(1);

  QSettings settings;
}

VideoWidget::~VideoWidget()
{}

void VideoWidget::inputImage(std::unique_ptr<uchar[]> input,
                             QImage &image)
{
  Q_ASSERT(input);
  drawMutex_.lock();
  input_ = std::move(input);
  currentImage_ = image;
  hasImage_ = true;
  drawMutex_.unlock();
  updated_ = true;

  if(previousSize_ != image.size())
  {
    qDebug() << "WARNING: Probably calling QFrame update from wrong thread!";
    updateTargetRect();
  }
}

void VideoWidget::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  //qDebug() << "PaintEvent for widget:" << id_;
  QPainter painter(this);

  if(hasImage_)
  {
    if(updated_)
    {
      drawMutex_.lock();
      Q_ASSERT(input_);

      painter.drawImage(targetRect_, currentImage_);
      drawMutex_.unlock();
    }
  }
  else
  {
    painter.fillRect(event->rect(), QBrush(QColor(0,0,0)));
  }

  QFrame::paintEvent(event);
}

void VideoWidget::resizeEvent(QResizeEvent *event)
{
  qDebug() << "ResizeEvent:" << id_;
  QWidget::resizeEvent(event);
  updateTargetRect();
}

void VideoWidget::keyPressEvent(QKeyEvent *event)
{
  if(event->key() == Qt::Key_Escape)
  {
    qDebug() << "Esc key pressed";
    this->showNormal();
  }
  else
  {
    qDebug() << "You Pressed Other Key";
  }
}

void VideoWidget::updateTargetRect()
{
  if(hasImage_)
  {
    QSize size = currentImage_.size();
    QSize frameSize = QWidget::size() - QSize(borderSize_,borderSize_);

    if(frameSize.height() > size.height()
       && frameSize.width() > size.width())
    {
      size.scale(frameSize.expandedTo(size), Qt::KeepAspectRatio);
    }
    else
    {
       size.scale(frameSize.boundedTo(size), Qt::KeepAspectRatio);
    }

    targetRect_ = QRect(QPoint(0, 0), size);
    targetRect_.moveCenter(rect().center());
    QRect frameRect = QRect(QPoint(0, 0), size + QSize(borderSize_,borderSize_));
    frameRect.moveCenter(rect().center());

    QFrame::setFrameRect(frameRect);

    previousSize_ = currentImage_.size();
  }
}
