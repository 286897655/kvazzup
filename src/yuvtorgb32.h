#pragma once

#include "filter.h"


class YUVtoRGB32 : public Filter
{
public:
  YUVtoRGB32(QString id, StatisticsInterface* stats);

protected:

  void process();

};

