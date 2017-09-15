#include "fashiontrayitem.h"

#include <QPainter>
#include <QDebug>
#include <QMouseEvent>
#include <QPixmap>
#include <QSvgRenderer>
#include <QApplication>

#include <cmath>

#include <xcb/xproto.h>

#define DRAG_THRESHOLD  10

const double pi = std::acos(-1);

FashionTrayItem::FashionTrayItem(QWidget *parent)
    : QWidget(parent),
      m_enableMouseEvent(false),
      m_activeTray(nullptr)
{

}

TrayWidget *FashionTrayItem::activeTray()
{
    return m_activeTray;
}

void FashionTrayItem::setMouseEnable(const bool enable)
{
    m_enableMouseEvent = enable;
}

void FashionTrayItem::setActiveTray(TrayWidget *tray)
{
    if (m_activeTray)
    {
        m_activeTray->setActive(false);
        disconnect(m_activeTray, &TrayWidget::iconChanged, this, static_cast<void (FashionTrayItem::*)()>(&FashionTrayItem::update));
    }

    if (tray)
    {
        tray->setActive(true);
        connect(tray, &TrayWidget::iconChanged, this, static_cast<void (FashionTrayItem::*)()>(&FashionTrayItem::update));
    }

    m_activeTray = tray;
    update();
}

void FashionTrayItem::resizeEvent(QResizeEvent *e)
{
    // update icon size
    const auto ratio = qApp->devicePixelRatio();
    const QSize s = e->size() * ratio;
    m_backgroundPixmap = loadSvg(":/icons/resources/trayicon.svg", 0.8 * std::min(s.width(), s.height()));
    m_backgroundPixmap.setDevicePixelRatio(ratio);

    QWidget::resizeEvent(e);
}

void FashionTrayItem::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    const QRectF r = rect();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // draw blue circle
    const auto ratio = qApp->devicePixelRatio();
    const int x = r.center().x() - m_backgroundPixmap.rect().center().x() / ratio;
    const int y = r.center().y() - m_backgroundPixmap.rect().center().y() / ratio;
    painter.drawPixmap(x, y, m_backgroundPixmap);

    // draw active icon
    if (m_activeTray)
    {
        const QImage image = m_activeTray->trayImage();
        painter.drawImage(r.center() - image.rect().center(), image);
    }
}

void FashionTrayItem::mousePressEvent(QMouseEvent *e)
{
    const QPoint dis = e->pos() - rect().center();
    if (dis.manhattanLength() > std::min(width(), height()) / 2 * 0.8)
        return QWidget::mousePressEvent(e);

    if (e->button() != Qt::RightButton)
        QWidget::mousePressEvent(e);

    m_pressPoint = e->pos();
}

void FashionTrayItem::mouseReleaseEvent(QMouseEvent *e)
{
    QWidget::mouseReleaseEvent(e);

    const QPoint point = e->pos() - m_pressPoint;

    if (point.manhattanLength() > DRAG_THRESHOLD)
        return;

    if (!m_activeTray)
        return;

    if (!m_enableMouseEvent)
        return;

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

    m_activeTray->sendClick(buttonIndex, globalPos.x(), globalPos.y());
}

const QPixmap FashionTrayItem::loadSvg(const QString &fileName, const int size) const
{
    QPixmap pixmap(size, size);
    QSvgRenderer renderer(fileName);
    pixmap.fill(Qt::transparent);

    QPainter painter;
    painter.begin(&pixmap);
    renderer.render(&painter);
    painter.end();

    return pixmap;
}
