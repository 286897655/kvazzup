#pragma once

#include <QFrame>

#include <deque>

class ChartPainter : public QFrame
{
    Q_OBJECT
public:
  ChartPainter(QWidget *parent);
  ~ChartPainter();

  void init(int maxY, int yLines, int xWindowSize);

  // return line ID
  int addLine(QString name);

  // add one point to line
  void addPoint(int lineID, float y);

  void clearPoints();

protected:
  void paintEvent(QPaintEvent *event);
  void resizeEvent(QResizeEvent *event);
  void keyPressEvent(QKeyEvent *event);

  void mouseDoubleClickEvent(QMouseEvent *e);

private:

  void drawBackground(QPainter& painter);
  void drawPoints(QPainter& painter, int lineID);
  void drawForeground(QPainter& painter);

  int getDrawMinX() const;
  int getDrawMaxX() const;

  int getDrawMinY() const;
  int getDrawMaxY() const;

  int maxY_;
  int xWindowCount_;

  int numberHeight_;
  int numberWidth_;

  int yLines_;

  //std::deque<float> points_;

  std::vector<std::shared_ptr<std::deque<float>>> points_;

  QStringList names_;
};
