#pragma once
#include "filter.h"

enum RETURN_STATUS {C_SUCCESS = 0, C_FAILURE = -1};

struct kvz_api;
struct kvz_config;
struct kvz_encoder;
struct kvz_picture;

class KvazaarFilter : public Filter
{
public:
  KvazaarFilter(QString id, StatisticsInterface* stats);

  int init(QSize resolution,
           int32_t framerate_num,
           int32_t framerate_denom);

  void close();

protected:
  virtual void process();

private:
  const kvz_api *api_;
  kvz_config *config_;
  kvz_encoder *enc_;

  int64_t pts_;

  kvz_picture *input_pic_;
};
