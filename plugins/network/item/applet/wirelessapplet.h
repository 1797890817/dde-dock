/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef WIRELESSAPPLET_H
#define WIRELESSAPPLET_H

#include "devicecontrolwidget.h"
#include "accesspoint.h"
#include "../../networkdevice.h"
#include "../../dbus/dbusnetwork.h"

#include <QScrollArea>
#include <QVBoxLayout>
#include <QList>
#include <QTimer>
#include <QCheckBox>
#include <dpicturesequenceview.h>
#include <dinputdialog.h>

DWIDGET_USE_NAMESPACE

class AccessPointWidget;
class WirelessList : public QScrollArea
{
    Q_OBJECT

public:
    explicit WirelessList(const QSet<NetworkDevice>::const_iterator &deviceIter, QWidget *parent = 0);
    ~WirelessList();

    NetworkDevice::NetworkState wirelessState() const;
    int activeAPStrgength() const;
    QWidget *controlPanel();

signals:
    void wirelessStateChanged(const NetworkDevice::NetworkState state) const;
    void activeAPChanged() const;

private:
    void setDeviceInfo(const int index);
    void loadAPList();

private slots:
    void init();
    void APAdded(const QString &devPath, const QString &info);
    void APRemoved(const QString &devPath, const QString &info);
    void APPropertiesChanged(const QString &devPath, const QString &info);
    void updateAPList();
    void deviceEnableChanged(const bool enable);
    void deviceStateChanged();
    void onActiveAPChanged();
    void pwdDialogAccepted();
    void pwdDialogCanceled();
    void onPwdDialogTextChanged(const QString &text);
    void deviceEnabled(const QString &devPath, const bool enable);
    void activateAP(const QDBusObjectPath &apPath, const QString &ssid);
    void deactiveAP();
    void needSecrets(const QString &info);
    void updateIndicatorPos();


private:
    NetworkDevice m_device;

    AccessPoint m_activeAP;
    QList<AccessPoint> m_apList;
    QList<AccessPointWidget*> m_apwList;

    QTimer *m_updateAPTimer;
    Dtk::Widget::DInputDialog *m_pwdDialog;
    QCheckBox *m_autoConnBox;
    Dtk::Widget::DPictureSequenceView *m_indicator;
    AccessPointWidget *m_currentClickAPW;
    AccessPoint m_currentClickAP;

    QString m_lastConnPath;
    QString m_lastConnSecurity;
    QString m_lastConnSecurityType;

    QVBoxLayout *m_centralLayout;
    QWidget *m_centralWidget;
    DeviceControlWidget *m_controlPanel;
    DBusNetwork *m_networkInter;
    bool m_deviceEnabled;

};

#endif // WIRELESSAPPLET_H