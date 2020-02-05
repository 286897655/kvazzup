#pragma once

#include "ui/settings/customsettings.h"
#include "ui/settings/advancedsettings.h"

#include <QDialog>
#include <QSettings>

#include <memory>

#include "serverstatusview.h"

/* Settings in Kvazzup work as follows:
 * 1) The settings view holds the setting information in a way that the user
 * can modify it. 2) This Settings class monitors users modifications and
 * records them to a file using QSettings. The file is loaded when the user
 * opens the settings dialog. 3) The rest of the program may use these settings
 * through QSettings class to change its behavior based on users choices.
 *
 * In other words this class synchronizes the settings between UI,
 * QSettings and the settings file (through QSettings).
 *
 * Modifying the settings are done in these settings classes and reading can
 * be done anywhere in the program.
 */

namespace Ui {
class BasicSettings;
}

class CameraInfo;
class QCheckBox;
class QComboBox;

// TODO: Settings of SIP server
class Settings : public QDialog, public ServerStatusView
{
  Q_OBJECT

public:
  explicit Settings(QWidget *parent = nullptr);
  ~Settings();

  void init();

  void updateDevices();

  void updateServerStatus(ServerStatus status);

signals:

  // announces to rest of the program that they should reload their settings
  void settingsChanged();

public slots:

  virtual void show();

  // button slots, called automatically by Qt
  void on_save_clicked();
  void on_close_clicked();
  void on_advanced_settings_button_clicked();
  void on_custom_settings_button_clicked();

  void changedSIPText(const QString &text);

private:
  void initializeUIDeviceList(QComboBox* deviceSelector,
                              QString settingID,
                              QString settingsDevice);

  // checks if user settings make sense
  // TODO: display errors to user on ok click
  bool checkUserSettings();
  bool checkMissingValues();

  // QSettings -> GUI
  void getSettings(bool changedDevice);

  // GUI -> QSettings
  // permanently records GUI settings
  void saveSettings();

  QStringList getAudioDevices();

  // Make sure the UI video devices are initialized before calling this.
  // This function tries to get the best guess at what is the current device
  // even in case devices have dissappeared/appeared since recording of information.
  int getDeviceID(QComboBox *deviceSelector, QString settingID, QString settingsDevice);

  void resetFaultySettings();

  Ui::BasicSettings *basicUI_;

  std::shared_ptr<CameraInfo> cam_;

  AdvancedSettings advanced_;
  CustomSettings custom_;

  QSettings settings_;
};
