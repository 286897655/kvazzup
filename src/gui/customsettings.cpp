#include "customsettings.h"

#include "ui_advancedsettings.h"

#include <video/camerainfo.h>

#include <QDebug>

CustomSettings::CustomSettings(QWidget* parent,
                               std::shared_ptr<CameraInfo> info)
  :
  QDialog(parent),
  currentDevice_(0),
  advancedUI_(new Ui::AdvancedSettings),
  cam_(info),
  settings_("kvazzup.ini", QSettings::IniFormat)
{
  advancedUI_->setupUi(this);
  // the buttons are named so that the slots are called automatically

  QObject::connect(advancedUI_->format_box, SIGNAL(activated(int)), this, SLOT(initializeResolutions(int)));
}

void CustomSettings::init(int deviceID)
{
  currentDevice_ = deviceID;
  initializeFormat();
  restoreAdvancedSettings();
}

void CustomSettings::initializeFormat()
{
  qDebug() << "Initializing formats";
  QStringList formats;
  QList<QStringList> resolutions;
  cam_->getVideoCapabilities(currentDevice_, formats, resolutions);

  advancedUI_->format_box->clear();
  for(int i = 0; i < formats.size(); ++i)
  {
    advancedUI_->format_box->addItem( formats.at(i));
  }
  int formatIndex = getFormatIndex();
  advancedUI_->format_box->setCurrentIndex(formatIndex);

  initializeResolutions(formatIndex);
}

int CustomSettings::getFormatIndex()
{
  int formatIndex = advancedUI_->format_box->findText(settings_.value("video/InputFormat").toString());
  int formatID = settings_.value("video/InputFormatID").toInt();

  if(formatID != formatIndex)
  {
    qDebug() << "Resettings resolution and format, because the format index was not correct. Index:" << formatIndex
             << "ID:" << formatID;
    // the formats have changed (device has changed), reset both format and resolution.
    settings_.setValue("video/InputFormatID",   formatIndex);
    settings_.setValue("video/ResolutionID",    0);
    settings_.setValue("video/CapabilityID",    cam_->formatToCapability(currentDevice_, formatIndex, 0));
  }

  if(formatIndex == -1)
  {
    formatIndex = 0;
  }
  return formatIndex;
}

void CustomSettings::initializeResolutions(int index)
{
  qDebug() << "Initializing resolutions";
  advancedUI_->resolution->clear();
  QStringList formats;
  QList<QStringList> resolutions;
  cam_->getVideoCapabilities(currentDevice_, formats, resolutions);

  for(int i = 0; i < resolutions.at(index).size(); ++i)
  {
    advancedUI_->resolution->addItem(resolutions.at(index).at(i));
  }

  int resolutionID = settings_.value("video/ResolutionID").toInt();

  advancedUI_->resolution->setCurrentIndex(resolutionID);
}

void CustomSettings::changedDevice(uint16_t deviceIndex)
{
  currentDevice_ = deviceIndex;
  initializeFormat();
  saveCameraCapabilities(deviceIndex); // record the new camerasettings.
}

void CustomSettings::resetSettings(int deviceID)
{
  qDebug() << "Resetting custom settings from UI";
  currentDevice_ = deviceID;
  initializeFormat();
  saveAdvancedSettings();
}

void CustomSettings::on_custom_ok_clicked()
{
  qDebug() << "Saving advanced settings";
  saveAdvancedSettings();
  emit customSettingsChanged();
  emit hidden();
  hide();
}

void CustomSettings::on_custom_cancel_clicked()
{
  qDebug() << "Cancelled modifying custom settings. Getting settings from system";
  restoreAdvancedSettings();
  hide();
  emit hidden();
}

void CustomSettings::show()
{
  qDebug() << "Showing custom settings";
  // no need to initialize format/resolutions because they only change when device is changed.
  QWidget::show();
}

void CustomSettings::saveAdvancedSettings()
{
  qDebug() << "Saving advanced Settings";

  // Video settings
  saveTextValue("video/Threads",               advancedUI_->threads->text());
  saveTextValue("video/OPENHEVC_threads",      advancedUI_->openhevc_threads->text());
  saveTextValue("video/VPS",                   advancedUI_->vps->text());
  saveTextValue("video/Intra",                 advancedUI_->intra->text());
  saveCheckBox("video/WPP",                    advancedUI_->wpp);
  saveCheckBox("video/Slices",                 advancedUI_->slices);

  settings_.setValue("video/QP",               QString::number(advancedUI_->qp->value()));
  settings_.setValue("video/Preset",           advancedUI_->preset->currentText());

  saveCameraCapabilities(settings_.value("video/DeviceID").toInt());
  //settings.sync(); // TODO is this needed?
}

void CustomSettings::saveCameraCapabilities(int deviceIndex)
{
  qDebug() << "Recording capability settings for deviceIndex:" << deviceIndex;
  QSize resolution = QSize(0,0);
  double fps = 0.0f;
  QString format = "";

  int formatIndex = advancedUI_->format_box->currentIndex();
  int resolutionIndex = advancedUI_->resolution->currentIndex();

  qDebug() << "Boxes in following positions: Format:" << formatIndex << "Resolution:" << resolutionIndex;
  if(formatIndex == -1)
  {
    formatIndex = 0;
  }

  if(resolutionIndex == -1)
  {
    resolutionIndex = 0;
  }

  cam_->getCapability(deviceIndex, formatIndex, resolutionIndex, resolution, fps, format);
  int32_t fps_int = static_cast<int>(fps);

  // since kvazaar requires resolution to be divisible by eight
  // TODO: Use QSize to record resolution
  settings_.setValue("video/ResolutionWidth",      resolution.width() - resolution.width()%8);
  settings_.setValue("video/ResolutionHeight",     resolution.height() - resolution.height()%8);
  settings_.setValue("video/ResolutionID",         resolutionIndex);
  settings_.setValue("video/CapabilityID",         cam_->formatToCapability(currentDevice_, formatIndex, resolutionIndex));
  settings_.setValue("video/Framerate",            fps_int);
  settings_.setValue("video/InputFormat",          format);
  settings_.setValue("video/InputFormatID",        formatIndex);

  qDebug() << "Recorded the following video settings: Resolution:"
           << resolution.width() - resolution.width()%8 << "x" << resolution.height() - resolution.height()%8
           << "fps:" << fps_int << "resolution index:" << resolutionIndex << "format" << format;
}

void CustomSettings::restoreAdvancedSettings()
{
  if(checkVideoSettings())
  {
    qDebug() << "Restoring previous Advanced settings from file:" << settings_.fileName();
    int index = advancedUI_->preset->findText(settings_.value("video/Preset").toString());
    if(index != -1)
      advancedUI_->preset->setCurrentIndex(index);
    advancedUI_->threads->setText        (settings_.value("video/Threads").toString());
    advancedUI_->qp->setValue            (settings_.value("video/QP").toInt());
    advancedUI_->openhevc_threads->setText        (settings_.value("video/OPENHEVC_threads").toString());

    restoreCheckBox("video/WPP", advancedUI_->wpp);

    advancedUI_->vps->setText            (settings_.value("video/VPS").toString());
    advancedUI_->intra->setText          (settings_.value("video/Intra").toString());

    restoreCheckBox("video/Slices", advancedUI_->slices);

    int formatIndex = getFormatIndex();
    advancedUI_->format_box->setCurrentIndex(formatIndex);

    int resolutionID = settings_.value("video/ResolutionID").toInt();
    advancedUI_->resolution->setCurrentIndex(resolutionID);

  }
  else
  {
    resetSettings(currentDevice_);
  }
}

void CustomSettings::restoreCheckBox(const QString settingValue, QCheckBox* box)
{
  if(settings_.value(settingValue).toString() == "1")
  {
    box->setChecked(true);
  }
  else if(settings_.value(settingValue).toString() == "0")
  {
    box->setChecked(false);
  }
  else
  {
    qDebug() << "Corrupted value for checkbox in settings file for:" << settingValue << "!!!";
  }
}

void CustomSettings::saveCheckBox(const QString settingValue, QCheckBox* box)
{
  if(box->isChecked())
  {
    settings_.setValue(settingValue,          "1");
  }
  else
  {
    settings_.setValue(settingValue,          "0");
  }
}

void CustomSettings::saveTextValue(const QString settingValue, const QString &text)
{
  if(text != "")
  {
    settings_.setValue(settingValue,  text);
  }
}

bool CustomSettings::checkMissingValues()
{
  QStringList list = settings_.allKeys();

  bool foundEverything = true;
  for(auto key : list)
  {
    if(settings_.value(key).isNull() || settings_.value(key) == "")
    {
      qDebug() << "MISSING SETTING FOR:" << key;
      foundEverything = false;
    }
  }
  return foundEverything;
}

bool CustomSettings::checkVideoSettings()
{
  return checkMissingValues()
      && settings_.contains("video/DeviceID")
      && settings_.contains("video/Device")
      && settings_.contains("video/ResolutionWidth")
      && settings_.contains("video/ResolutionHeight")
      && settings_.contains("video/WPP")
      && settings_.contains("video/Framerate")
      && settings_.contains("video/InputFormat")
      && settings_.contains("video/Slices");
}
