#pragma once

#include <QWaitCondition>
#include <QThread>
#include <QMutex>

#include <sys/time.h>
#include <cstdint>
#include <vector>
#include <queue>
#include <memory>
#include <functional>

enum DataType {NONE = 0, RGB32VIDEO, YUVVIDEO, RAWAUDIO, HEVCVIDEO, OPUSAUDIO};
enum DataSource {UNKNOWN, LOCAL, REMOTE};

// NOTE: remember to make changes to deepcopy, camera, audiocapture and RTPSinkFilter
struct Data
{
  uint8_t type;
  std::unique_ptr<uchar[]> data;
  uint32_t data_size;
  int16_t width;
  int16_t height;
  timeval presentationTime;

  uint16_t framerate;

  DataSource source;
};

const int BUFFERSIZE = 2;
class StatisticsInterface;

class Filter : public QThread
{
  Q_OBJECT

public:
  Filter(QString id, QString name, StatisticsInterface* stats, bool input, bool output);
  virtual ~Filter();

  virtual void updateSettings();

  // adds one outbound connection to this filter.
  void addOutConnection(Filter *out);
  void removeOutConnection(Filter *out);

  // callback registeration enables other classes besides Filter
  // to receive output data
  template <typename Class>
  void addDataOutCallback (Class* o, void (Class::*method) (std::unique_ptr<Data> data))
  {
    outDataCallbacks_.push_back([o,method] (std::unique_ptr<Data> data)
    {
      return (o->*method)(std::move(data));
    });
  }

  // empties the input buffer
  void emptyBuffer();

  void putInput(std::unique_ptr<Data> data);

  // for debugging filter graphs
  virtual bool isInputFilter() const
  {
    return input_;
  }
  virtual bool isOutputFilter() const
  {
    return output_;
  }

  virtual void start()
  {
    QThread::start();
  }

  virtual void stop();

  QString printOutputs();

  // helper function for copying Data
  Data* shallowDataCopy(Data* original);
  Data* deepDataCopy(Data* original);

protected:

  // return: oldest element in buffer, empty if none found
  std::unique_ptr<Data> getInput();

  //sends output to out connections
  void sendOutput(std::unique_ptr<Data> output);

  // QThread function that runs the processing
  void run();

  virtual void process() = 0;

  void sleep()
  {
    waitMutex_->lock();
    hasInput_.wait(waitMutex_);
    waitMutex_->unlock();
  }

  QString name_;
  StatisticsInterface* stats_;

private:

  QMutex *waitMutex_;
  QWaitCondition hasInput_;

  bool running_;

  std::vector<std::function<void(std::unique_ptr<Data>)> > outDataCallbacks_;

  QMutex connectionMutex_;
  std::vector<Filter*> outConnections_;

  QMutex bufferMutex_;
  std::queue<std::unique_ptr<Data>> inBuffer_;

  unsigned int inputTaken_;
  unsigned int inputDiscarded_;

  bool input_;
  bool output_;
};
