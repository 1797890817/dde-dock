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

#include "powertraywidget.h"

#include <QPainter>
#include <QIcon>
#include <QMouseEvent>

PowerTrayWidget::PowerTrayWidget(AbstractTrayWidget *parent)
    : AbstractTrayWidget(parent),
      m_powerInter(new DBusPower(this))
{
    connect(m_powerInter, &DBusPower::BatteryPercentageChanged, this, &PowerTrayWidget::updateIcon);
    connect(m_powerInter, &DBusPower::BatteryStateChanged, this, &PowerTrayWidget::updateIcon);
    connect(m_powerInter, &DBusPower::OnBatteryChanged, this, &PowerTrayWidget::updateIcon);
}

void PowerTrayWidget::setActive(const bool active)
{

}

void PowerTrayWidget::updateIcon()
{
    const BatteryPercentageMap data = m_powerInter->batteryPercentage();
    const uint value = qMin(100.0, qMax(0.0, data.value("Display")));
    const int percentage = std::round(value);
    const bool plugged = !m_powerInter->onBattery();

    QString percentageStr;
    if (percentage < 10 && percentage >= 0) {
        percentageStr = "000";
    } else if (percentage < 30) {
        percentageStr = "020";
    } else if (percentage < 50) {
        percentageStr = "040";
    } else if (percentage < 70) {
        percentageStr = "060";
    } else if (percentage < 90) {
        percentageStr = "080";
    } else if (percentage <= 100){
        percentageStr = "100";
    } else {
        percentageStr = "000";
    }

    const QString iconStr = QString("battery-%1-%2")
                                .arg(percentageStr)
                                .arg(plugged ? "plugged-symbolic" : "symbolic");
    const auto ratio = devicePixelRatioF();
    m_pixmap = QIcon::fromTheme(iconStr).pixmap(QSize(16, 16) * ratio);
    m_pixmap.setDevicePixelRatio(ratio);

    update();
}

void PowerTrayWidget::sendClick(uint8_t mouseButton, int x, int y)
{

}

const QImage PowerTrayWidget::trayImage()
{
    m_pixmap.toImage();
}

QSize PowerTrayWidget::sizeHint() const
{
    return QSize(26, 26);
}

void PowerTrayWidget::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    const auto ratio = devicePixelRatioF();

    QPainter painter(this);
    painter.drawPixmap(rect().center() - m_pixmap.rect().center() / ratio, m_pixmap);
}
