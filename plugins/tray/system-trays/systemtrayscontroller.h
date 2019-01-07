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

#ifndef SYSTEMTRAYSCONTROLLER_H
#define SYSTEMTRAYSCONTROLLER_H

#include "systemtrayitem.h"
#include "pluginproxyinterface.h"

#include <QPluginLoader>
#include <QList>
#include <QMap>
#include <QDBusConnectionInterface>

class PluginsItemInterface;
class SystemTraysController : public QObject, PluginProxyInterface
{
    Q_OBJECT

public:
    explicit SystemTraysController(QObject *parent = nullptr);

    // implements PluginProxyInterface
    void itemAdded(PluginsItemInterface * const itemInter, const QString &itemKey) Q_DECL_OVERRIDE;
    void itemUpdate(PluginsItemInterface * const itemInter, const QString &itemKey) Q_DECL_OVERRIDE;
    void itemRemoved(PluginsItemInterface * const itemInter, const QString &itemKey) Q_DECL_OVERRIDE;
    void requestWindowAutoHide(PluginsItemInterface * const itemInter, const QString &itemKey, const bool autoHide) Q_DECL_OVERRIDE;
    void requestRefreshWindowVisible(PluginsItemInterface * const itemInter, const QString &itemKey) Q_DECL_OVERRIDE;
    void requestSetAppletVisible(PluginsItemInterface * const itemInter, const QString &itemKey, const bool visible) Q_DECL_OVERRIDE;
    void saveValue(PluginsItemInterface *const itemInter, const QString &key, const QVariant &value) Q_DECL_OVERRIDE;
    const QVariant getValue(PluginsItemInterface *const itemInter, const QString &key, const QVariant& failback = QVariant()) Q_DECL_OVERRIDE;

    int systemTrayItemSortKey(const QString &itemKey);
    void setSystemTrayItemSortKey(const QString &itemKey, const int order);

public slots:
    void startLoader();

signals:
    void systemTrayAdded(const QString &itemKey, AbstractTrayWidget *trayWidget) const;
    void systemTrayRemoved(const QString &itemKey) const;
    void systemTrayUpdated(const QString &itemKey) const;

private slots:
    void displayModeChanged();
    void positionChanged();
    void loadPlugin(const QString &pluginFile);
    void initPlugin(PluginsItemInterface *interface);

private:
    bool eventFilter(QObject *o, QEvent *e) Q_DECL_OVERRIDE;
    SystemTrayItem *pluginItemAt(PluginsItemInterface * const itemInter, const QString &itemKey) const;
    PluginsItemInterface *pluginInterAt(const QString &itemKey) const;
    PluginsItemInterface *pluginInterAt(SystemTrayItem *systemTrayItem) const;

private:
    QDBusConnectionInterface *m_dbusDaemonInterface;
    QMap<PluginsItemInterface *, QMap<QString, SystemTrayItem *>> m_pluginsMap;

    QSettings m_pluginsSetting;
};

#endif // SYSTEMTRAYSCONTROLLER_H
