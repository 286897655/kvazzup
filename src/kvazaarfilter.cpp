#include "kvazaarfilter.h"

#include <kvazaar.h>
#include <QtDebug>

#include <QSize>


KvazaarFilter::KvazaarFilter(StatisticsInterface *stats):
  Filter("Kvazaar", stats),
  api_(NULL),
  config_(NULL),
  enc_(NULL),
  pts_(0),
  input_pic_(NULL)
{}

int KvazaarFilter::init(QSize resolution,
                        int32_t framerate_num,
                        int32_t framerate_denom,
                        float target_bitrate)
{
  qDebug() << name_ << "iniating";

  // input picture should not exist at this point
  Q_ASSERT(!input_pic_ && !api_);

  api_ = kvz_api_get(8);
  if(!api_)
  {
    qCritical() << name_ << "failed to retreive Kvazaar API";
    return C_FAILURE;
  }
  config_ = api_->config_alloc();
  enc_ = NULL;

  if(!config_)
  {
    qCritical() << name_ << "failed to allocate Kvazaar config";
    return C_FAILURE;
  }

  api_->config_init(config_);
  api_->config_parse(config_, "preset", "ultrafast");
  config_->width = resolution.width();
  config_->height = resolution.height();
  config_->threads = 4;
  config_->qp = 32;
  config_->wpp = 1;
  config_->vps_period = 1;
  config_->intra_period = 64;



  //config_->target_bitrate = target_bitrate;

  enc_ = api_->encoder_open(config_);

  if(!enc_)
  {
    qCritical() << name_ << "failed to open Kvazaar encoder";
    return C_FAILURE;
  }

  input_pic_ = api_->picture_alloc(config_->width, config_->height);

  if(!input_pic_)
  {
    qCritical() << name_ << "could not allocate input picture";
    return C_FAILURE;
  }

  qDebug() << name_ << "iniation success";
  return C_SUCCESS;
}

void KvazaarFilter::close()
{
  if(api_)
  {
    if(input_pic_)
      api_->picture_free(input_pic_);

    input_pic_ = NULL;
    api_->encoder_close(enc_);
    api_->config_destroy(config_);
    enc_ = NULL;
    config_ = NULL;
  }

}

void KvazaarFilter::process()
{
  Q_ASSERT(enc_);
  Q_ASSERT(config_);

  std::unique_ptr<Data> input = getInput();
  while(input)
  {
    kvz_picture *recon_pic = NULL;
    kvz_frame_info frame_info;
    kvz_data_chunk *data_out = NULL;
    uint32_t len_out = 0;

    if(config_->width != input->width
       || config_->height != input->height)
    {
      qCritical() << name_ << "unhandled change of resolution";
      break;
    }


    if(!input_pic_)
    {
      qCritical() << name_ << "input picture was not allocated correctly";
      break;
    }

    memcpy(input_pic_->y,
           input->data.get(),
           input->width*input->height);
    memcpy(input_pic_->u,
           &(input->data.get()[input->width*input->height]),
           input->width*input->height/4);
    memcpy(input_pic_->v,
           &(input->data.get()[input->width*input->height + input->width*input->height/4]),
           input->width*input->height/4);

    input_pic_->pts = pts_;
    ++pts_;

    api_->encoder_encode(enc_, input_pic_,
                         &data_out, &len_out,
                         &recon_pic, NULL,
                         &frame_info );

    if(data_out != NULL)
    {

      Data *hevc_frame = new Data;
      hevc_frame->data_size = len_out;
      hevc_frame->data = std::unique_ptr<uchar[]>(new uchar[len_out]);
      hevc_frame->width = input->width;
      hevc_frame->height = input->height;
      hevc_frame->type = HEVCVIDEO;

      uint8_t* writer = hevc_frame->data.get();

      kvz_data_chunk *previous_chunk = 0;
      for (kvz_data_chunk *chunk = data_out; chunk != NULL; chunk = chunk->next)
      {
        memcpy(writer, chunk->data, chunk->len);
        writer += chunk->len;
        previous_chunk = chunk;
      }
      api_->chunk_free(data_out);
      api_->picture_free(recon_pic);

      std::unique_ptr<Data> hevc_frame_data( hevc_frame );

      qDebug() << "Frame encoded. Size:" << hevc_frame_data->data_size
               << " width:" << hevc_frame_data->width << ", height:" << hevc_frame_data->height;
      sendOutput(std::move(hevc_frame_data));

    }
    input = getInput();
  }
}
