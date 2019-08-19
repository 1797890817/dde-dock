/*
 * Copyright (C) 2019 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     xuwenw <xuwenw@xuwenw.so>
 *
 * Maintainer:  <@xuwenw.so>
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

#ifndef MAINPANELCONTROL_H
#define MAINPANELCONTROL_H

#include <QWidget>
#include <QBoxLayout>

class DockItem;
class MainPanelControl : public QWidget
{
    Q_OBJECT
public:
    MainPanelControl(QWidget *parent = 0);
    ~MainPanelControl();

    void addFixedAreaItem(const int index, QWidget *wdg);
    void addAppAreaItem(const int index, QWidget *wdg);
    void addTrayAreaItem(const int index, QWidget *wdg);
    void addPluginAreaItem(const int index, QWidget *wdg);
    void removeFixedAreaItem(QWidget *wdg);
    void removeAppAreaItem(QWidget *wdg);
    void removeTrayAreaItem(QWidget *wdg);
    void removePluginAreaItem(QWidget *wdg);
    void setPositonValue(const Qt::Edge val);

private:
    void resizeEvent(QResizeEvent *event) override;

    void init();
    void updateAppAreaSonWidgetSize();
    void updateMainPanelLayout();

public slots:
    void insertItem(const int index, DockItem *item);
    void removeItem(DockItem *item);
    void movedItem(const int index, DockItem *item);

private:
    QBoxLayout *m_mainPanelLayout;
    QWidget *m_fixedAreaWidget;
    QWidget *m_appAreaWidget;
    QWidget *m_trayAreaWidget;
    QWidget *m_pluginAreaWidget;
    QBoxLayout *m_fixedAreaLayout;
    QBoxLayout *m_trayAreaLayout;
    QBoxLayout *m_pluginLayout;
    QWidget *m_appAreaSonWidget;
    QBoxLayout *m_appAreaSonLayout;
    Qt::Edge m_position;
};

#endif // MAINPANELCONTROL_H
