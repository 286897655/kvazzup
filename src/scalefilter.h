#pragma once

#include "filter.h"

#include <QSize>

class ScaleFilter : public Filter
{
public:
  ScaleFilter(QString id, StatisticsInterface *stats);

  void setResolution(QSize newResolution);

  void process();

  std::unique_ptr<Data> scaleFrame(std::unique_ptr<Data> input);

private:

  QSize newSize_;
};
