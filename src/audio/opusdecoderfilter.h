#pragma once
#include "filter.h"

#include <opus.h>
#include <QAudioFormat>

class OpusDecoderFilter : public Filter
{
public:
  OpusDecoderFilter(QString id, QAudioFormat format, StatisticsInterface* stats);
  ~OpusDecoderFilter();

  // setups decoder
  virtual bool init();

protected:

  // decodes input until buffer is empty
  void process();

private:

  OpusDecoder *dec_;

  int16_t* pcmOutput_;
  uint32_t max_data_bytes_;

  QAudioFormat format_;
};
