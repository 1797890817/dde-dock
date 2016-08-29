#ifndef WIRELESSITEM_H
#define WIRELESSITEM_H

#include "constants.h"

#include "deviceitem.h"
#include "applet/wirelessapplet.h"

#include <QHash>

class WirelessItem : public DeviceItem
{
    Q_OBJECT

public:
    explicit WirelessItem(const QUuid &uuid);
    ~WirelessItem();

    NetworkDevice::NetworkType type() const;
    NetworkDevice::NetworkState state() const;
    QWidget *itemApplet();

protected:
    bool eventFilter(QObject *o, QEvent *e);
    void paintEvent(QPaintEvent *e);
    void resizeEvent(QResizeEvent *e);
    void mousePressEvent(QMouseEvent *e);

private:
    const QPixmap iconPix(const Dock::DisplayMode displayMode, const int size);
    const QPixmap backgroundPix(const int size);
    const QPixmap cachedPix(const QString &key, const int size);

private slots:
    void init();
    void adjustHeight();

private:
    QHash<QString, QPixmap> m_icons;

    QTimer *m_refershTimer;
    QWidget *m_wirelessApplet;
    WirelessList *m_APList;
};

#endif // WIRELESSITEM_H
