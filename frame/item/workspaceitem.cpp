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

#include "workspaceitem.h"
#include "util/themeappicon.h"
#include "util/imagefactory.h"

#include <QPainter>
#include <QProcess>
#include <QMouseEvent>
#include <DDBusSender>
#include <QApplication>
#include <QGSettings>

DCORE_USE_NAMESPACE

WorkSpaceItem::WorkSpaceItem(QWidget *parent)
    : DockItem(parent)
//    , m_launcherInter(new LauncherInter("com.deepin.dde.Launcher", "/com/deepin/dde/Launcher", QDBusConnection::sessionBus(), this))
//    , m_tips(new TipsWidget(this))
    , m_gsettings(new QGSettings("com.deepin.dde.dock.module.workspace"))
{
//    m_launcherInter->setSync(true, false);

//    m_tips->setVisible(false);
//    m_tips->setObjectName("launcher");
//    m_tips->setAccessibleName("launchertips");

    connect(m_gsettings, &QGSettings::changed, this, &WorkSpaceItem::onGSettingsChanged);
}

void WorkSpaceItem::refershIcon()
{
    update();
}

void WorkSpaceItem::showEvent(QShowEvent* event) {
    QTimer::singleShot(0, this, [=] {
        onGSettingsChanged("enable");
    });

    return DockItem::showEvent(event);
}

void WorkSpaceItem::paintEvent(QPaintEvent *e)
{
    DockItem::paintEvent(e);

    if (!isVisible())
        return;

    QPainter painter(this);

    int iconSize = qMin(width(), height());
    if (DockDisplayMode == Efficient)
    {
        iconSize = iconSize * 0.7;
    } else {
        iconSize = iconSize * 0.8;
    }

    QColor color = this->palette().color(QPalette::Base);
    color.setAlpha(100);

    painter.fillRect(rect().marginsRemoved(QMargins(10,10,10,10)),color);
}

void WorkSpaceItem::resizeEvent(QResizeEvent *e)
{
    DockItem::resizeEvent(e);

    refershIcon();
}

void WorkSpaceItem::mousePressEvent(QMouseEvent *e)
{
    if (checkGSettingsControl()) {
        return;
    }

//    hidePopup();

    return QWidget::mousePressEvent(e);
}

void WorkSpaceItem::mouseReleaseEvent(QMouseEvent *e)
{
    if (checkGSettingsControl()) {
        return;
    }

    if (e->button() != Qt::LeftButton)
        return;

//    if (!m_launcherInter->IsVisible()) {
//        m_launcherInter->Show();
//    }
}

//QWidget *WorkSpaceItem::popupTips()
//{
//    if (checkGSettingsControl()) {
//        return nullptr;
//    }

////    m_tips->setText(tr("Launcher"));
//    return nullptr;
//}

void WorkSpaceItem::onGSettingsChanged(const QString& key) {
    if (key != "enable") {
        return;
    }

    if (m_gsettings->keys().contains("enable")) {
        setVisible(m_gsettings->get("enable").toBool());
    }
}

bool WorkSpaceItem::checkGSettingsControl() const
{
    return m_gsettings->keys().contains("control")
            && m_gsettings->get("control").toBool();
}
