#include "appitem.h"

#include "util/themeappicon.h"
#include "util/imagefactory.h"
#include "xcb/xcb_misc.h"

#include <QPainter>
#include <QDrag>
#include <QMouseEvent>

#define APP_DRAG_THRESHOLD      20

int AppItem::IconBaseSize;
QPoint AppItem::MousePressPos;

AppItem::AppItem(const QDBusObjectPath &entry, QWidget *parent)
    : DockItem(parent),
      m_appNameTips(new QLabel(this)),
      m_itemEntry(new DBusDockEntry(entry.path(), this)),
      m_draging(false),
      m_horizontalIndicator(QPixmap(":/indicator/resources/indicator.png")),
      m_verticalIndicator(QPixmap(":/indicator/resources/indicator_ver.png")),
      m_activeHorizontalIndicator(QPixmap(":/indicator/resources/indicator_active.png")),
      m_activeVerticalIndicator(QPixmap(":/indicator/resources/indicator_active_ver.png")),
      m_updateIconGeometryTimer(new QTimer(this))
{
    setAccessibleName(m_itemEntry->name());
    setAcceptDrops(true);

    m_id = m_itemEntry->id();
    m_active = m_itemEntry->active();

    m_appNameTips->setObjectName(m_itemEntry->name());
    m_appNameTips->setVisible(false);
    m_appNameTips->setStyleSheet("color:white;"
                                 "padding:5px 10px;");

    m_updateIconGeometryTimer->setInterval(500);
    m_updateIconGeometryTimer->setSingleShot(true);

    connect(m_itemEntry, &DBusDockEntry::ActiveChanged, this, &AppItem::activeChanged);
    connect(m_itemEntry, &DBusDockEntry::TitlesChanged, this, &AppItem::updateTitle);
    connect(m_itemEntry, &DBusDockEntry::ActiveChanged, this, static_cast<void (AppItem::*)()>(&AppItem::update));

    connect(m_updateIconGeometryTimer, &QTimer::timeout, this, &AppItem::updateWindowIconGeometries);

    updateTitle();
    updateIcon();
}

const QString AppItem::appId() const
{
    return m_id;
}

// Update _NET_WM_ICON_GEOMETRY property for windows that every item
// that manages, so that WM can do proper animations for specific
// window behaviors like minimization.
void AppItem::updateWindowIconGeometries()
{
    QRect rect(mapToGlobal(QPoint(0, 0)),
               mapToGlobal(QPoint(width(),height())));

    if (rect != m_lastGlobalGeometry) {
        QList<quint32> winIds = m_titles.keys();
        for (quint32 winId : winIds) {
            XcbMisc::instance()->set_window_icon_geometry(winId, rect);
        }
        m_lastGlobalGeometry = rect;
    }
}

void AppItem::setIconBaseSize(const int size)
{
    IconBaseSize = size;
}

int AppItem::iconBaseSize()
{
    return IconBaseSize;
}

int AppItem::itemBaseWidth()
{
    if (DockDisplayMode == Dock::Fashion)
        return itemBaseHeight() * 1.1;
    else
        return itemBaseHeight() * 1.4;
}

int AppItem::itemBaseHeight()
{
    if (DockDisplayMode == Efficient)
        return IconBaseSize * 1.2;
    else
        return IconBaseSize * 1.5;
}

void AppItem::paintEvent(QPaintEvent *e)
{
    DockItem::paintEvent(e);

    if (m_draging)
        return;

    QPainter painter(this);
    if (!painter.isActive())
        return;

    const QRect itemRect = rect();

    // draw background
    QRect backgroundRect = itemRect;
    if (DockDisplayMode == Efficient)
    {
        switch (DockPosition)
        {
        case Top:
        case Bottom:
//            backgroundRect = itemRect;//.marginsRemoved(QMargins(2, 0, 2, 0));
//            backgroundRect = itemRect.marginsRemoved(QMargins(0, 1, 0, 1));
        case Left:
        case Right:
            backgroundRect = itemRect.marginsRemoved(QMargins(1, 1, 1, 1));
//            backgroundRect = itemRect.marginsRemoved(QMargins(1, 0, 1, 0));
        }
    }

    if (DockDisplayMode == Efficient)
    {
        if (m_active)
        {
            painter.fillRect(backgroundRect, QColor(44, 167, 248, 255 * 0.3));

            const int activeLineWidth = itemRect.height() > 50 ? 4 : 2;
            QRect activeRect = backgroundRect;
            switch (DockPosition)
            {
            case Top:       activeRect.setBottom(activeRect.top() + activeLineWidth);   break;
            case Bottom:    activeRect.setTop(activeRect.bottom() - activeLineWidth);   break;
            case Left:      activeRect.setRight(activeRect.left() + activeLineWidth);   break;
            case Right:     activeRect.setLeft(activeRect.right() - activeLineWidth);   break;
            }

            painter.fillRect(activeRect, QColor(44, 167, 248, 255));
        }
        else if (!m_titles.isEmpty())
            painter.fillRect(backgroundRect, QColor(255, 255, 255, 255 * 0.2));
    //    else
    //        painter.fillRect(backgroundRect, Qt::gray);
    }
    else
    {
        if (!m_titles.isEmpty())
        {
            QPoint p;
            QPixmap pixmap;
            QPixmap activePixmap;
            switch (DockPosition)
            {
            case Top:
                pixmap = m_horizontalIndicator;
                activePixmap = m_activeHorizontalIndicator;
                p.setX((itemRect.width() - pixmap.width()) / 2);
                p.setY(1);
                break;
            case Bottom:
                pixmap = m_horizontalIndicator;
                activePixmap = m_activeHorizontalIndicator;
                p.setX((itemRect.width() - pixmap.width()) / 2);
                p.setY(itemRect.height() - pixmap.height() - 1);
                break;
            case Left:
                pixmap = m_verticalIndicator;
                activePixmap = m_activeVerticalIndicator;
                p.setX(1);
                p.setY((itemRect.height() - pixmap.height()) / 2);
                break;
            case Right:
                pixmap = m_verticalIndicator;
                activePixmap = m_activeVerticalIndicator;
                p.setX(itemRect.width() - pixmap.width() - 1);
                p.setY((itemRect.height() - pixmap.height()) / 2);
                break;
            }

            if (m_active)
                painter.drawPixmap(p, activePixmap);
            else
                painter.drawPixmap(p, pixmap);

//            const int activeLineWidth = 2;
//            const int activeLineLength = 20;
//            QRect activeRect = itemRect;
//            switch (DockPosition)
//            {
//            case Top:
//                activeRect.setBottom(activeRect.top() + activeLineWidth);
//                activeRect.moveBottom(activeRect.bottom() + 1);
//                activeRect.setWidth(activeLineLength);
//                activeRect.moveLeft((itemRect.width() - activeRect.width()) / 2);
//                break;
//            case Bottom:
//                activeRect.setTop(activeRect.bottom() - activeLineWidth);
//                activeRect.moveTop(activeRect.top() - 1);
//                activeRect.setWidth(activeLineLength);
//                activeRect.moveLeft((itemRect.width() - activeRect.width()) / 2);
//                break;
//            case Left:
//                activeRect.setRight(activeRect.left() + activeLineWidth);
//                activeRect.moveRight(activeRect.right() + 1);
//                activeRect.setHeight(activeLineLength);
//                activeRect.moveTop((itemRect.height() - activeRect.height()) / 2);
//                break;
//            case Right:
//                activeRect.setLeft(activeRect.right() - activeLineWidth);
//                activeRect.moveLeft(activeRect.left() - 1);
//                activeRect.setHeight(activeLineLength);
//                activeRect.moveTop((itemRect.height() - activeRect.height()) / 2);
//                break;
//            }

//            painter.fillRect(activeRect, QColor(163, 167, 166));
        }
    }

    // icon
    const QPixmap pixmap = DockDisplayMode == Efficient ? m_smallIcon : m_largeIcon;
    // draw icon
    painter.drawPixmap(itemRect.center() - pixmap.rect().center(), pixmap);
    // draw ligher
    if (m_hover)
        painter.drawPixmap(itemRect.center() - pixmap.rect().center(), ImageFactory::lighterEffect(pixmap));

    // Update the window icon geometry when the icon is changed.
    m_updateIconGeometryTimer->start();
}

void AppItem::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;

    const QPoint distance = MousePressPos - e->pos();
    if (distance.manhattanLength() < APP_DRAG_THRESHOLD)
        m_itemEntry->Activate();
}

void AppItem::mousePressEvent(QMouseEvent *e)
{
    m_updateIconGeometryTimer->stop();
    hidePopup();

    if (e->button() == Qt::RightButton)
    {
        if (perfectIconRect().contains(e->pos()))
            return showContextMenu();
        else
            return QWidget::mousePressEvent(e);
    }

    if (e->button() == Qt::LeftButton)
        MousePressPos = e->pos();
}

void AppItem::mouseMoveEvent(QMouseEvent *e)
{
    e->accept();

    // handle drag
    if (e->buttons() != Qt::LeftButton)
        return;

    const QPoint distance = e->pos() - MousePressPos;
    if (distance.manhattanLength() < APP_DRAG_THRESHOLD)
        return;

    startDrag();
}

void AppItem::resizeEvent(QResizeEvent *e)
{
    DockItem::resizeEvent(e);

    updateIcon();
}

void AppItem::dragEnterEvent(QDragEnterEvent *e)
{
    // ignore drag from panel
    if (e->source())
        return;

    // ignore request dock event
    if (e->mimeData()->formats().contains("RequestDock"))
        return e->ignore();

    e->accept();
}

void AppItem::dropEvent(QDropEvent *e)
{
    QStringList uriList;
    for (auto uri : e->mimeData()->urls())
        uriList << uri.toString();

//    qDebug() << uriList;
    m_itemEntry->HandleDragDrop(uriList);
}

void AppItem::invokedMenuItem(const QString &itemId, const bool checked)
{
    Q_UNUSED(checked);

    m_itemEntry->HandleMenuItem(itemId);
}

const QString AppItem::contextMenu() const
{
    return m_itemEntry->menu();
}

QWidget *AppItem::popupTips()
{
    if (m_draging)
        return nullptr;

    if (!m_titles.isEmpty())
    {
        const quint32 currentWindow = m_itemEntry->currentWindow();
        Q_ASSERT(m_titles.contains(currentWindow));
        m_appNameTips->setText(m_titles[currentWindow]);
    } else {
        m_appNameTips->setText(m_itemEntry->name());
    }

    return m_appNameTips;
}

void AppItem::startDrag()
{
    m_draging = true;
    update();

    const QPixmap dragPix = DockDisplayMode == Dock::Fashion ? m_largeIcon : m_smallIcon;

    QDrag *drag = new QDrag(this);
    drag->setPixmap(dragPix);
    drag->setHotSpot(dragPix.rect().center());
    drag->setMimeData(new QMimeData);

    emit dragStarted();
    const Qt::DropAction result = drag->exec(Qt::MoveAction);
    Q_UNUSED(result);

    // drag out of dock panel
    if (!drag->target())
        m_itemEntry->RequestUndock();

    m_draging = false;
    setVisible(true);
    update();
}

void AppItem::updateTitle()
{
    m_titles = m_itemEntry->titles();

    update();
}

void AppItem::updateIcon()
{
    const QString icon = m_itemEntry->icon();
    const int iconSize = qMin(width(), height());

    if (DockDisplayMode == Efficient)
    {
        m_smallIcon = ThemeAppIcon::getIcon(icon, iconSize * 0.7);
        m_largeIcon = ThemeAppIcon::getIcon(icon, iconSize * 0.9);
    } else {
        m_smallIcon = ThemeAppIcon::getIcon(icon, iconSize * 0.6);
        m_largeIcon = ThemeAppIcon::getIcon(icon, iconSize * 0.8);
    }
}

void AppItem::activeChanged()
{
    m_active = !m_active;
}
