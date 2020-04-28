#include "filtergraph.h"

#include <QFile>

#include "media/processing/camerafilter.h"
#include "media/processing/screensharefilter.h"
#include "media/processing/kvazaarfilter.h"
#include "media/processing/rgb32toyuv.h"
#include "media/processing/openhevcfilter.h"
#include "media/processing/yuvtorgb32.h"
#include "media/processing/displayfilter.h"
#include "media/processing/scalefilter.h"
#include "media/processing/audiocapturefilter.h"
#include "media/processing/audiooutput.h"
#include "media/processing/opusencoderfilter.h"
#include "media/processing/opusdecoderfilter.h"
#include "media/processing/aecinputfilter.h"
#include "media/processing/aecplaybackfilter.h"
#include "media/processing/audiomixerfilter.h"

#include "ui/gui/videointerface.h"

#include "global.h"
#include "common.h"

#include <QSettings>

FilterGraph::FilterGraph():
  peers_(),
  cameraGraph_(),
  screenShareGraph_(),
  audioProcessing_(),
  selfView_(nullptr),
  stats_(nullptr),
  format_(),
  quitting_(false),
  aec_(),
  audioOutput_(nullptr)
{
  // TODO negotiate these values with all included filters and SDP
  // TODO move these to settings and manage them automatically
  // TODO: at the moment uvgRTP does not support larger audio frames for pcm
  // 48000 should be used with opus
  format_.setSampleRate(16000); 
  //format_.setSampleRate(48000);
  format_.setChannelCount(1);
  format_.setSampleSize(16);
  format_.setSampleType(QAudioFormat::SignedInt);
  format_.setByteOrder(QAudioFormat::LittleEndian);
  format_.setCodec("audio/pcm");
}


void FilterGraph::init(VideoInterface* selfView, StatisticsInterface* stats)
{
  Q_ASSERT(stats);

  stats_ = stats;
  selfView_ = selfView;

  initSelfView(selfView);
}


void FilterGraph::updateSettings()
{
  QSettings settings("kvazzup.ini", QSettings::IniFormat);
  // if the video format has changed so that we need different conversions
  if(videoFormat_ != settings.value("video/InputFormat").toString())
  {
    qDebug() << "Settings, FilterGraph :" << "The video format has been changed from " << videoFormat_
             << "to" << settings.value("video/InputFormat").toString() << "Video send graph has to be reconstructed.";

    // update selfview in case camera format has changed
    initSelfView(selfView_);

    // if we are in a call, initiate kvazaar and connect peers. Otherwise add it late.
    if(peers_.size() != 0)
    {
      initVideoSend();

      // reconnect all videosends to streamers
      for(Peer* peer : peers_)
      {
        if(peer != nullptr)
        {
          for (auto& senderFilter : peer->videoSenders)
          {
            cameraGraph_.back()->addOutConnection(senderFilter);
          }
        }
      }
    }
  }
  else
  {
    for(auto& filter : cameraGraph_)
    {
      filter->updateSettings();
    }
  }

  for(auto& filter : audioProcessing_)
  {
    filter->updateSettings();
  }

  for(Peer* peer : peers_)
  {
    if(peer != nullptr)
    {
      for (auto& senderFilter : peer->videoSenders)
      {
        senderFilter->updateSettings();
      }
      for (auto& senderFilter : peer->audioSenders)
      {
        senderFilter->updateSettings();
      }

      // decode and display settings
      for(auto& videoReceivers : peer->videoReceivers)
      {
        for (auto& filter : *videoReceivers)
        {
          filter->updateSettings();
        }
      }

      for(auto& audioReceivers : peer->audioReceivers)
      {
        for (auto& filter : *audioReceivers)
        {
          filter->updateSettings();
        }
      }
    }
  }
}


void FilterGraph::initSelfView(VideoInterface *selfView)
{
  printDebug(DEBUG_NORMAL, "FilterGraph", "Iniating camera and selfview");

  if(cameraGraph_.size() > 0)
  {
    destroyFilters(cameraGraph_);
  }

  // Sending video graph

  if (!addToGraph(std::shared_ptr<Filter>(new CameraFilter("", stats_)), cameraGraph_))
  {
    // camera failed
    printDebug(DEBUG_ERROR, "FilterGraph", "Failed to add camera. Does it have supported formats.");
    return; // TODO: return false that we failed so user can fix camera selection
  }

  if(screenShareGraph_.size() == 0)
  {
    if (addToGraph(std::shared_ptr<Filter>(new ScreenShareFilter("", stats_)), screenShareGraph_))
    {
      screenShareGraph_.at(0)->stop();
    }
  }

  if(selfView)
  {
    // connect scaling filter
    // TODO: not useful if it does not support YUV. Testing needed to verify
    /*
    ScaleFilter* scaler = new ScaleFilter("Self", stats_);
    scaler->setResolution(selfView->size());
    addToGraph(std::shared_ptr<Filter>(scaler), videoSend_);
    */

    // connect selfview to camera
    std::shared_ptr<DisplayFilter> selfviewFilter = std::shared_ptr<DisplayFilter>(new DisplayFilter("Self", stats_, selfView, 1111));
    // the self view rotation depends on which conversions are use as some of the optimizations
    // do the mirroring. Note: mirroring is slow with Qt
    selfviewFilter->setProperties(true, cameraGraph_.at(0)->outputType() == RGB32VIDEO);
    addToGraph(selfviewFilter, cameraGraph_);
    addToGraph(selfviewFilter, screenShareGraph_);
  }
}


void FilterGraph::initVideoSend()
{
  qDebug() << "Session construction," << "FilterGraph :" << "Iniating video send";

  if(cameraGraph_.size() == 0)
  {
    qDebug() << "WARNING: FilterGraph : Camera was not iniated for video send";
    initSelfView(selfView_);
  }
  else if(cameraGraph_.size() > 3)
  {
    qDebug() << "WARNING: FilterGraph : Too many filters in videosend";
    destroyFilters(cameraGraph_);
  }

  std::shared_ptr<Filter> kvazaar = std::shared_ptr<Filter>(new KvazaarFilter("", stats_));
  addToGraph(kvazaar, cameraGraph_, 0);
  addToGraph(cameraGraph_.back(), screenShareGraph_, 0);
}


void FilterGraph::initAudioSend(bool opus)
{
  // Do this before adding participants, otherwise AEC filter wont get attached
  addToGraph(std::shared_ptr<Filter>(new AudioCaptureFilter("", format_, stats_)), audioProcessing_);

  if (AEC_ENABLED)
  {
    aec_ = std::shared_ptr<AECInputFilter>(new AECInputFilter("", stats_));
    aec_->initInput(format_);
    addToGraph(aec_, audioProcessing_, audioProcessing_.size() - 1);
  }

  if (opus)
  {
    addToGraph(std::shared_ptr<Filter>(new OpusEncoderFilter("", format_, stats_)), audioProcessing_,
               audioProcessing_.size() - 1);
  }
}


bool FilterGraph::addToGraph(std::shared_ptr<Filter> filter,
                             GraphSegment &graph,
                             unsigned int connectIndex)
{
  // // check if we need some sort of conversion and connect to index
  if(graph.size() > 0 && connectIndex <= graph.size() - 1)
  {
    if(graph.at(connectIndex)->outputType() != filter->inputType())
    {
      qDebug() << "FilterGraph : Filter output and input do not match for" << graph.at(connectIndex)->getName()
               << "->" << filter->getName() << "Trying to find an existing conversion:"
               << graph.at(connectIndex)->outputType() << "to" << filter->inputType();

      Q_ASSERT(graph.at(connectIndex)->outputType() != NONE);

      // TODO: Check the out connections of connected filter for an already existing conversion.

      if(graph.at(connectIndex)->outputType() == RGB32VIDEO &&
         filter->inputType() == YUV420VIDEO)
      {
        qDebug() << "FilterGraph : Found RGB32 to YUV conversion needed";
        addToGraph(std::shared_ptr<Filter>(new RGB32toYUV("", stats_)),
                   graph, connectIndex);
      }
      else if(graph.at(connectIndex)->outputType() == YUV420VIDEO &&
              filter->inputType() == RGB32VIDEO)
      {
        qDebug() << "FilterGraph : Found RGB32 to YUV conversion needed";
        addToGraph(std::shared_ptr<Filter>(new YUVtoRGB32("", stats_)),
                   graph, connectIndex);
      }
      else
      {
        printDebug(DEBUG_WARNING, "FilterGraph", 
                   "Could not find conversion for filter.");
        return false;
      }
      // the conversion filter has been added to the end
      connectIndex = graph.size() - 1;
    }
    connectFilters(filter, graph.at(connectIndex));
  }

  graph.push_back(filter);
  if(filter->init())
  {
    filter->start();
  }
  else
  {
    return false;
  }
  //qDebug() << "FilterGraph : Added, iniated and started" << filter->getName();
  return true;
}


bool FilterGraph::connectFilters(std::shared_ptr<Filter> filter, std::shared_ptr<Filter> previous)
{
  Q_ASSERT(filter != nullptr && previous != nullptr);

  printDebug(DEBUG_NORMAL, "FilterGraph", "Connecting filters",
              {"Connection"}, {previous->getName() + " -> " + filter->getName()});

  if(previous->outputType() != filter->inputType())
  {
    printDebug(DEBUG_WARNING, "FilterGraph", 
               "The connecting filter output and input DO NOT MATCH.");
    return false;
  }
  previous->addOutConnection(filter);
  return true;
}


void FilterGraph::checkParticipant(uint32_t sessionID)
{
  Q_ASSERT(stats_);
  Q_ASSERT(sessionID);

  //qDebug() << "FilterGraph : Checking participant with session ID:" << sessionID;

  if(peers_.size() >= sessionID)
  {
    if(peers_.at(sessionID - 1) != nullptr)
    {
      qDebug() << "FilterGraph: Peer exists for session ID:" << sessionID;
      return;
    }
    else
    {
      qDebug() << "FilterGraph: Replacing old participant with session ID:" << sessionID;
      peers_.at(sessionID - 1) = new Peer;
    }
  }
  else
  {
    while(peers_.size() < sessionID)
    {
      peers_.push_back(nullptr);
    }
    peers_.at(sessionID - 1) = new Peer();

    qDebug() << "FilterGraph: Adding participant to end with sessionID:" << sessionID;
  }

  peers_.at(sessionID - 1)->audioSenders.clear();
  peers_.at(sessionID - 1)->videoSenders.clear();
}


void FilterGraph::sendVideoto(uint32_t sessionID, std::shared_ptr<Filter> videoFramedSource)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(videoFramedSource);

  qDebug() << "Session construction, FilterGraph: Adding send video for peer:" << sessionID;

  // make sure we are generating video
  if(cameraGraph_.size() < 4)
  {
    initVideoSend();
  }

  // add participant if necessary
  checkParticipant(sessionID);

  peers_.at(sessionID - 1)->videoSenders.push_back(videoFramedSource);

  cameraGraph_.back()->addOutConnection(videoFramedSource);
  videoFramedSource->start();
}


void FilterGraph::receiveVideoFrom(uint32_t sessionID, std::shared_ptr<Filter> videoSink,
                                   VideoInterface *view)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(videoSink);

  checkParticipant(sessionID);

  std::shared_ptr<GraphSegment> graph = std::shared_ptr<GraphSegment> (new GraphSegment);
  peers_.at(sessionID - 1)->videoReceivers.push_back(graph);

  addToGraph(videoSink, *graph);
  addToGraph(std::shared_ptr<Filter>(new OpenHEVCFilter(QString::number(sessionID), stats_)),
             *graph, 0);

  addToGraph(std::shared_ptr<Filter>(new DisplayFilter(QString::number(sessionID), stats_,
                                                       view, sessionID)),
             *graph, 1);
}


void FilterGraph::sendAudioTo(uint32_t sessionID, std::shared_ptr<Filter> audioFramedSource)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(audioFramedSource);

  // just in case it is wanted later. AEC filter has to be attached
  if(audioProcessing_.size() == 0)
  {
    initAudioSend(audioFramedSource->inputType() == OPUSAUDIO);
  }

  // add participant if necessary
  checkParticipant(sessionID);


  peers_.at(sessionID - 1)->audioSenders.push_back(audioFramedSource);

  audioProcessing_.back()->addOutConnection(audioFramedSource);
  audioFramedSource->start();
}


void FilterGraph::receiveAudioFrom(uint32_t sessionID, std::shared_ptr<Filter> audioSink)
{
  Q_ASSERT(sessionID);
  Q_ASSERT(audioSink);

  if (audioOutput_ == nullptr)
  {
    audioOutput_ = std::make_shared<AudioOutput>(stats_);
    audioOutput_->initializeAudio(format_);
  }

  // just in case it is wanted later. AEC filter has to be attached
  if(AEC_ENABLED && audioProcessing_.size() == 0)
  {
    initAudioSend(audioSink->outputType() == OPUSAUDIO);
  }

  // add participant if necessary
  checkParticipant(sessionID);

  std::shared_ptr<GraphSegment> graph = std::shared_ptr<GraphSegment> (new GraphSegment);
  peers_.at(sessionID - 1)->audioReceivers.push_back(graph);

  addToGraph(audioSink, *graph);
  if (audioSink->outputType() == OPUSAUDIO)
  {
    addToGraph(std::shared_ptr<Filter>(new OpusDecoderFilter(QString::number(sessionID),
                                                             format_, stats_)),
               *graph, graph->size() - 1);
  }

  if(audioProcessing_.size() > 0 && AEC_ENABLED)
  {
    addToGraph(std::shared_ptr<Filter>(
                 new AECPlaybackFilter(QString::number(sessionID), stats_, sessionID,
                                       aec_->getAEC())),
               *graph, graph->size() - 1);
  }
  else
  {
    printDebug(DEBUG_WARNING, "FilterGraph",
               "Did not attach echo cancellation");
  }

  audioOutput_->addInput();

  if (AEC_ENABLED)
  {
    addToGraph(std::make_shared<AudioMixerFilter>(QString::number(sessionID),
                                                  stats_, sessionID, audioOutput_),
               *graph, graph->size() - 2);
  }
  else
  {
    addToGraph(std::make_shared<AudioMixerFilter>(QString::number(sessionID),
                                                  stats_, sessionID, audioOutput_),
               *graph, graph->size() - 1);
  }
}


void FilterGraph::uninit()
{
  quitting_ = true;
  removeAllParticipants();

  destroyFilters(cameraGraph_);
  destroyFilters(screenShareGraph_);
  destroyFilters(audioProcessing_);
}


void FilterGraph::removeAllParticipants()
{
  for(unsigned int i = 0; i < peers_.size(); ++i)
  {
    if(peers_.at(i) != nullptr)
    {
      removeParticipant(i + 1);
    }
  }
  peers_.clear();
}


void changeState(std::shared_ptr<Filter> f, bool state)
{
  if(state)
  {
    f->emptyBuffer();
    f->start();
  }
  else
  {
    f->stop();
    f->emptyBuffer();

    while(f->isRunning())
    {
      qSleep(1);
    }
  }
}


void FilterGraph::mic(bool state)
{
  if(audioProcessing_.size() > 0)
  {
    if(!state)
    {
      qDebug() << "FilterGraph : Stopping microphone";
      audioProcessing_.at(0)->stop();
    }
    else
    {
      qDebug() << "FilterGraph : Starting microphone";
      audioProcessing_.at(0)->start();
    }
  }
}


void FilterGraph::camera(bool state)
{
  if(cameraGraph_.size() > 0)
  {
    if(!state)
    {
      qDebug() << "FilterGraph : Stopping camera";
      cameraGraph_.at(0)->stop();
    }
    else
    {
      qDebug() << "FilterGraph : Starting camera";
      cameraGraph_.at(0)->start();
    }
  }
}


void FilterGraph::screenShare(bool shareState, bool cameraState)
{
  if(cameraGraph_.size() > 0 && screenShareGraph_.size() > 0)
  {
    if(shareState)
    {
      qDebug() << "FilterGraph : Starting to share screen";
      screenShareGraph_.at(0)->start();
      cameraGraph_.at(0)->stop();
    }
    else
    {
      qDebug() << "FilterGraph : Stopping to share screen";
      screenShareGraph_.at(0)->stop();
      camera(cameraState);
    }
  }
}


void FilterGraph::running(bool state)
{
  for(std::shared_ptr<Filter> f : cameraGraph_)
  {
    changeState(f, state);
  }
  for(std::shared_ptr<Filter> f : audioProcessing_)
  {
    changeState(f, state);
  }

  if (screenShareGraph_.size() > 0)
  {
    changeState(screenShareGraph_.at(0), state);
  }

  for(Peer* p : peers_)
  {
    if(p != nullptr)
    {
      for (auto& senderFilter : p->audioSenders)
      {
        if(senderFilter)
        {
          changeState(senderFilter, state);
        }
      }

      for (auto& senderFilter : p->videoSenders)
      {
        if(senderFilter)
        {
          changeState(senderFilter, state);
        }
      }

      for (auto& graph : p->audioReceivers)
      {
        for(std::shared_ptr<Filter> f : *graph)
        {
          changeState(f, state);
        }
      }

      for (auto& graph : p->videoReceivers)
      {
        for(std::shared_ptr<Filter> f : *graph)
        {
          changeState(f, state);
        }
      }
    }
  }
}


void FilterGraph::destroyFilters(std::vector<std::shared_ptr<Filter> > &filters)
{
  if(filters.size() != 0)
  {
    qDebug() << "Closing, FilterGraph :"
             << "Destroying filter graph with" << filters.size() << "filters.";
  }
  for( std::shared_ptr<Filter> f : filters )
  {
    changeState(f, false);
  }

  filters.clear();
}


void FilterGraph::destroyPeer(Peer* peer)
{
  qDebug() << "Remove participant, Filter Graph: Destroying peer from Filter Graph";

  for (auto& audioSender : peer->audioSenders)
  {
    audioProcessing_.back()->removeOutConnection(audioSender);
    //peer->audioFramedSource is destroyed by RTPStreamer
    changeState(audioSender, false);
    audioSender = nullptr;

    if(AEC_ENABLED)
    {
      for (auto& graph : peer->audioReceivers)
      {
        audioProcessing_.at(0)->removeOutConnection(graph->back());
      }
    }
  }
  for (auto& videoSender : peer->videoSenders)
  {
    cameraGraph_.back()->removeOutConnection(videoSender);
    changeState(videoSender, false);
    //peer->videoFramedSource is destroyed by RTPStreamer
    videoSender = nullptr;
  }
  for (auto& graph : peer->audioReceivers)
  {
    destroyFilters(*graph);
    audioOutput_->removeInput();
  }

  for (auto& graph : peer->videoReceivers)
  {
    destroyFilters(*graph);
  }

  delete peer;
}


void FilterGraph::removeParticipant(uint32_t sessionID)
{
  qDebug() << "Remove participant, FilterGraph : Removing peer:" << sessionID << "/" << peers_.size();
  Q_ASSERT(sessionID <= peers_.size());
  if(peers_.at(sessionID - 1) != nullptr)
    destroyPeer(peers_.at(sessionID - 1));
  peers_.at(sessionID - 1) = nullptr;

    // destroy send graphs if this was the last peer
  bool peerPresent = false;
  for(auto& peer : peers_)
  {
    if (peer != nullptr)
      peerPresent = true;
  }

  if(!peerPresent)
  {
    destroyFilters(cameraGraph_);
    if (!quitting_)
    {
      initSelfView(selfView_); // restore the self view.
    }

    destroyFilters(audioProcessing_);
  }
}


void FilterGraph::print()
{
  QString audioDotFile = "digraph AudioGraph {\r\n";

  for(auto& f : audioProcessing_)
  {
    audioDotFile += f->printOutputs();
  }

  for(unsigned int i = 0; i <peers_.size(); ++i)
  {
    if(peers_.at(i) != nullptr)
    {
      for(auto& graph : peers_.at(i)->audioReceivers)
      {
        for (auto& filter : *graph)
        {
          audioDotFile += filter->printOutputs();
        }

      }
      for (auto& audioSender : peers_.at(i)->audioSenders)
      {
        audioDotFile += audioSender->printOutputs();
      }
    }
  }
  audioDotFile += "}";

  QString videoDotFile = "digraph VideoGraph {\r\n";

  for(auto& f : cameraGraph_)
  {
    videoDotFile += f->printOutputs();
  }

  for(unsigned int i = 0; i <peers_.size(); ++i)
  {
    if(peers_.at(i) != nullptr)
    {
      for(auto&graph : peers_.at(i)->videoReceivers)
      {
        for (auto&filter : *graph)
        {
          videoDotFile += filter->printOutputs();
        }
      }

      for (auto&videoSender : peers_.at(i)->videoSenders)
      {
        audioDotFile += videoSender->printOutputs();
      }
    }
  }

  videoDotFile += "}";

  QString aFilename="audiograph.dot";
  QFile aFile( aFilename );
  if ( aFile.open(QIODevice::WriteOnly) )
  {
      QTextStream stream( &aFile );
      stream << audioDotFile << endl;
  }

  QString vFilename="videograph.dot";
  QFile vFile( vFilename );
  if ( vFile.open(QIODevice::WriteOnly) )
  {
      QTextStream stream( &vFile );
      stream << videoDotFile << endl;
  }
}
