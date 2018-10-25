/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     listenerri <listenerri@gmail.com>
 *
 * Maintainer: listenerri <listenerri@gmail.com>
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

#include "abstractnetworktraywidget.h"

#include <DDBusSender>
#include <QJsonDocument>

using namespace dde::network;

AbstractNetworkTrayWidget::AbstractNetworkTrayWidget(dde::network::NetworkDevice *device, QWidget *parent)
    : AbstractSystemTrayWidget(parent),
      m_device(device),
      m_path(device->path())
{
}

QSize AbstractNetworkTrayWidget::sizeHint() const
{
    return QSize(26, 26);
}

const QString AbstractNetworkTrayWidget::contextMenu() const
{
    QList<QVariant> items;
    items.reserve(2);

    QMap<QString, QVariant> enable;
    enable["itemId"] = "enable";
    if (!m_device->enabled())
        enable["itemText"] = tr("Enable network");
    else
        enable["itemText"] = tr("Disable network");
    enable["isActive"] = true;
    items.push_back(enable);

    QMap<QString, QVariant> settings;
    settings["itemId"] = "settings";
    settings["itemText"] = tr("Network settings");
    settings["isActive"] = true;
    items.push_back(settings);

    QMap<QString, QVariant> menu;
    menu["items"] = items;
    menu["checkableMenu"] = false;
    menu["singleCheck"] = false;

    return QJsonDocument::fromVariant(menu).toJson();
}

void AbstractNetworkTrayWidget::invokedMenuItem(const QString &menuId, const bool checked)
{
    if (menuId == "settings")
        //QProcess::startDetached("dbus-send --print-reply --dest=com.deepin.dde.ControlCenter /com/deepin/dde/ControlCenter com.deepin.dde.ControlCenter.ShowModule \"string:network\"");
        DDBusSender()
                .service("com.deepin.dde.ControlCenter")
                .interface("com.deepin.dde.ControlCenter")
                .path("/com/deepin/dde/ControlCenter")
                .method("ShowModule")
                .arg(QString("network"))
                .call();

    else if (menuId == "enable")
        Q_EMIT requestSetDeviceEnable(m_path, !m_device->enabled());
}
