#include "filtergraph.h"


#include "camerafilter.h"
#include "displayfilter.h"
#include "kvazaarfilter.h"
#include "rgb32toyuv.h"
#include "openhevcfilter.h"
#include "yuvtorgb32.h"
#include "framedsourcefilter.h"
#include "displayfilter.h"


FilterGraph::FilterGraph():filters_()//, streamControl_()
{

}

void FilterGraph::init(VideoWidget* selfView, QSize resolution)
{
  streamer_.setPorts(15555,18888);
  streamer_.start();

  initSender(selfView, resolution);
}


void FilterGraph::initSender(VideoWidget *selfView, QSize resolution)
{
  // Sending video graph
  filters_.push_back(new CameraFilter(resolution));

  // connect selfview to camera
  DisplayFilter* selfviewFilter = new DisplayFilter(selfView);
  selfviewFilter->setProperties(true);
  filters_.push_back(selfviewFilter);
  filters_.at(0)->addOutConnection(filters_.back());
  filters_.back()->start();

  filters_.push_back(new RGB32toYUV());
  filters_.at(0)->addOutConnection(filters_.back());
  filters_.back()->start();

  KvazaarFilter* kvz = new KvazaarFilter();
  kvz->init(resolution, 15,1, 0);
  filters_.push_back(kvz);
  filters_.at(filters_.size() - 2)->addOutConnection(filters_.back());
  filters_.back()->start();

  encoderFilter_ = filters_.size() - 1;


}

ParticipantID FilterGraph::addParticipant(in_addr ip, uint16_t port, VideoWidget* view,
                                          bool wantsAudio, bool sendsAudio,
                                          bool wantsVideo, bool sendsVideo)
{
  if(port != 0)
    streamer_.setPorts(15555, port);

  PeerID peer = streamer_.addPeer(ip, true, true);
  if(wantsVideo)
  {
    Filter *framedSource = NULL;
    framedSource = streamer_.getSourceFilter(peer);

    filters_.push_back(framedSource);
    filters_.at(encoderFilter_)->addOutConnection(filters_.back());
    filters_.back()->start();
  }

  if(sendsVideo && view != NULL)
  {
    // Receiving video graph
    Filter* rtpSink = NULL;
    rtpSink = streamer_.getSinkFilter(peer);

    filters_.push_back(rtpSink);
    filters_.back()->start();

    OpenHEVCFilter* decoder =  new OpenHEVCFilter();
    decoder->init();
    filters_.push_back(decoder);
    filters_.at(filters_.size() - 2)->addOutConnection(filters_.back());
    filters_.back()->start();

    filters_.push_back(new YUVtoRGB32());
    filters_.at(filters_.size() - 2)->addOutConnection(filters_.back());
    filters_.back()->start();

    filters_.push_back(new DisplayFilter(view));
    filters_.at(filters_.size() - 2)->addOutConnection(filters_.back());
    filters_.back()->start();
  }
  else if(view == NULL)
    qWarning() << "Warn: wanted to receive video, but no view available";
}


void FilterGraph::uninit()
{
  deconstruct();
}


void FilterGraph::deconstruct()
{
  for( Filter *f : filters_ )
    delete f;

  filters_.clear();
}

void FilterGraph::restart()
{
  for(Filter* f : filters_)
    f->start();

  streamer_.start();
}

void FilterGraph::stop()
{
  for(Filter* f : filters_)
  {
    f->stop();
    f->emptyBuffer();
  }
  streamer_.stop();
}
