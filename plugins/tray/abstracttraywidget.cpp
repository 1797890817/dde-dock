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

#include "abstracttraywidget.h"

#include <xcb/xproto.h>
#include <QMouseEvent>
#include <QDebug>

AbstractTrayWidget::AbstractTrayWidget(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
{
}

AbstractTrayWidget::~AbstractTrayWidget()
{

}

void AbstractTrayWidget::mousePressEvent(QMouseEvent *event)
{
    // do not call Parent::mousePressEvent or the DockItem will catch this event
    // and show dock-context-menu immediately when right button of mouse is pressed in fashion mode
    if (event->buttons() == Qt::RightButton) {
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void AbstractTrayWidget::mouseReleaseEvent(QMouseEvent *e)
{
    const QPoint point(e->pos() - rect().center());
    if (point.manhattanLength() > 24)
        return;

    e->accept();

    QPoint globalPos = QCursor::pos();
    uint8_t buttonIndex = XCB_BUTTON_INDEX_1;

    switch (e->button()) {
    case Qt:: MiddleButton:
        buttonIndex = XCB_BUTTON_INDEX_2;
        break;
    case Qt::RightButton:
        buttonIndex = XCB_BUTTON_INDEX_3;
        break;
    default:
        break;
    }

    sendClick(buttonIndex, globalPos.x(), globalPos.y());

    // left mouse button clicked
    if (buttonIndex == XCB_BUTTON_INDEX_1) {
        Q_EMIT clicked();
    }
}
