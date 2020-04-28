#pragma once

#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

#include <QAudioFormat>
#include <QMutex>

#include <deque>
#include <memory>

// This class implements Speex Echo cancellation. After some testing I think
// it is implemented optimally, but it does not seem very good. It blocks some
// voices, but not nearly all of them. I guess it is better than nothing, but
// it could be replaced at some point. The Automatic gain control is enabled

class AECProcessor : public QObject
{
  Q_OBJECT
public:
  AECProcessor(QAudioFormat format);

  void init();
  void cleanup();

  std::unique_ptr<uchar[]> processInputFrame(std::unique_ptr<uchar[]> input,
                                             uint32_t dataSize);

  void processEchoFrame(std::shared_ptr<uchar[]> echo,
                        uint32_t dataSize);

  std::shared_ptr<uchar[]> createEmptyFrame(uint32_t size);

private:

  QAudioFormat format_;
  uint32_t samplesPerFrame_;

  SpeexPreprocessState *preprocessor_;
  SpeexEchoState *echo_state_;

  QMutex echoMutex_;
  uint32_t echoSize_;
  std::shared_ptr<uchar[]> echoSample_;
};
