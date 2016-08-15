
#include "wirelessitem.h"
#include "util/imageutil.h"

#include <QPainter>
#include <QMouseEvent>

WirelessItem::WirelessItem(const QUuid &uuid)
    : DeviceItem(uuid),
      m_applet(nullptr)
{
    QMetaObject::invokeMethod(this, "init", Qt::QueuedConnection);
}

NetworkDevice::NetworkType WirelessItem::type() const
{
    return NetworkDevice::Wireless;
}

NetworkDevice::NetworkState WirelessItem::state() const
{
    return m_applet->wirelessState();
}

QWidget *WirelessItem::itemApplet()
{
    return m_applet;
}

void WirelessItem::paintEvent(QPaintEvent *e)
{
    DeviceItem::paintEvent(e);

    const Dock::DisplayMode displayMode = qApp->property(PROP_DISPLAY_MODE).value<Dock::DisplayMode>();

    const int iconSize = displayMode == Dock::Fashion ? std::min(width(), height()) * 0.8 : 16;
    const QPixmap pixmap = iconPix(displayMode, iconSize);

    QPainter painter(this);
    if (displayMode == Dock::Fashion)
    {
        const QPixmap pixmap = backgroundPix(iconSize);
        painter.drawPixmap(rect().center() - pixmap.rect().center(), pixmap);
    }
    painter.drawPixmap(rect().center() - pixmap.rect().center(), pixmap);
}

void WirelessItem::resizeEvent(QResizeEvent *e)
{
    DeviceItem::resizeEvent(e);

    m_icons.clear();
}

void WirelessItem::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        return QWidget::mousePressEvent(e);

    const QPoint p(e->pos() - rect().center());
    if (p.manhattanLength() < std::min(width(), height()) * 0.8 * 0.5)
        return;

    return QWidget::mousePressEvent(e);
}

const QPixmap WirelessItem::iconPix(const Dock::DisplayMode displayMode, const int size)
{
    QString type;
    if (m_applet->wirelessState() != NetworkDevice::Activated)
        type = "disconnect";
    else
    {
        const int strength = m_applet->activeAPStrgength();
        if (strength == 100)
            type = "8";
        else
            type = QString::number(strength / 10 & ~0x1);
    }

    const QString key = QString("wireless-%1%2")
                                .arg(type)
                                .arg(displayMode == Dock::Fashion ? "" : "-symbolic");

    return cachedPix(key, size);
}

const QPixmap WirelessItem::backgroundPix(const int size)
{
    return cachedPix("wireless-background", size);
}

const QPixmap WirelessItem::cachedPix(const QString &key, const int size)
{
    if (!m_icons.contains(key))
        m_icons.insert(key, ImageUtil::loadSvg(":/wireless/resources/wireless/" + key + ".svg", size));

    return m_icons.value(key);
}

void WirelessItem::init()
{
    const auto devInfo = m_networkManager->device(m_deviceUuid);

    m_applet = new WirelessApplet(devInfo, this);
    m_applet->setObjectName("wireless-" + m_deviceUuid.toString());
    m_applet->setVisible(false);

    connect(m_applet, &WirelessApplet::activeAPChanged, this, static_cast<void (WirelessItem::*)()>(&WirelessItem::update));
    connect(m_applet, &WirelessApplet::wirelessStateChanged, this, static_cast<void (WirelessItem::*)()>(&WirelessItem::update));
}
