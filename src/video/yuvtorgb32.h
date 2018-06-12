#pragma once
#include "filter.h"

class YUVtoRGB32 : public Filter
{
public:
  YUVtoRGB32(QString id, StatisticsInterface* stats, uint32_t peer);

protected:
  void process();

private:
  bool sse_;
  bool avx2_;
};

