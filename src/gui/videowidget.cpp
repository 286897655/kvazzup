#include "videowidget.h"

#include <QPaintEvent>
#include <QDebug>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>

unsigned int VideoWidget::number_ = 0;

uint16_t VIEWBUFFERSIZE = 3;

VideoWidget::VideoWidget(QWidget* parent, uint8_t borderSize): QFrame(parent),
  firstImageReceived_(false),
  previousSize_(QSize(0,0)),
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
}

VideoWidget::~VideoWidget()
{}

void VideoWidget::inputImage(QImage &image)
{
  drawMutex_.lock();
  viewBuffer_.push_front(image);

  // delete oldes image if there is too much buffer
  if(viewBuffer_.size() > VIEWBUFFERSIZE)
  {
    qDebug() << "WARNING: Buffer full. Deleting oldest image from viewBuffer in videowidget:" << id_;
    viewBuffer_.pop_back();
  }

  if(previousSize_ != image.size())
  {
    qDebug() << "Video widget needs to update its target rectangle because of resolution change.";
    viewBuffer_.clear();

    viewBuffer_.push_front(image);
    updateTargetRect();
  }
  drawMutex_.unlock();

  firstImageReceived_ = true;
  //update();
}

void VideoWidget::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  //qDebug() << "PaintEvent for widget:" << id_;
  QPainter painter(this);

  if(firstImageReceived_)
  {
    drawMutex_.lock();
    if(QFrame::frameRect() != newFrameRect_)
    {
      QFrame::setFrameRect(newFrameRect_);
      QWidget::setMinimumHeight(newFrameRect_.height()*QWidget::minimumWidth()/newFrameRect_.width());
    }

    if(!viewBuffer_.empty())
    {
      painter.drawImage(targetRect_, viewBuffer_.back());
      lastImage_ = viewBuffer_.back();
      viewBuffer_.pop_back();
    }
    else
    {
      painter.drawImage(targetRect_, lastImage_);
    }
    drawMutex_.unlock();
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
  // TODO: Find a way to

  if(!viewBuffer_.empty())
  {
    Q_ASSERT(viewBuffer_.back().data_ptr());
    if(viewBuffer_.back().data_ptr() == NULL)
    {
      qWarning() << "WARNING: Null pointer in current image!";
      return;
    }

    QSize size = viewBuffer_.back().size();
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
    newFrameRect_ = QRect(QPoint(0, 0), size + QSize(borderSize_,borderSize_));
    newFrameRect_.moveCenter(rect().center());

    previousSize_ = viewBuffer_.back().size();
  }
  else
  {
    qDebug() << "VideoWidget: Tried updating target rect before picture";
  }
}
