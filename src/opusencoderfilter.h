#pragma once

#include "filter.h"

#include <opus.h>

class OpusEncoderFilter : public Filter
{
public:
  OpusEncoderFilter(StatisticsInterface* stats);
  ~OpusEncoderFilter();

  void init();

protected:
  void process();

private:
  OpusEncoder* enc_;

  unsigned char* opusOutput_;
  uint32_t max_data_bytes_;
};
