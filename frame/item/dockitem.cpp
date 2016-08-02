
#include "dockitem.h"
#include "dbus/dbusmenu.h"
#include "dbus/dbusmenumanager.h"

#include <QMouseEvent>
#include <QJsonObject>

Position DockItem::DockPosition = Position::Top;
DisplayMode DockItem::DockDisplayMode = DisplayMode::Efficient;
std::unique_ptr<DockPopupWindow> DockItem::PopupWindow(nullptr);

DockItem::DockItem(const ItemType type, QWidget *parent)
    : QWidget(parent),
      m_type(type),
      m_hover(false),
      m_popupShown(false),

      m_popupTipsDelayTimer(new QTimer(this)),

      m_menuManagerInter(new DBusMenuManager(this))
{
    if (!PopupWindow.get())
    {
        DockPopupWindow *arrowRectangle = new DockPopupWindow(nullptr);
        arrowRectangle->setShadowBlurRadius(0);
        PopupWindow.reset(arrowRectangle);
    }

    m_popupTipsDelayTimer->setInterval(500);
    m_popupTipsDelayTimer->setSingleShot(true);

    connect(m_popupTipsDelayTimer, &QTimer::timeout, this, &DockItem::showHoverTips);
}

DockItem::~DockItem()
{
    if (m_popupShown)
        popupWindowAccept();
}

void DockItem::setDockPosition(const Position side)
{
    DockPosition = side;
}

void DockItem::setDockDisplayMode(const DisplayMode mode)
{
    DockDisplayMode = mode;
}

DockItem::ItemType DockItem::itemType() const
{
    return m_type;
}

void DockItem::paintEvent(QPaintEvent *e)
{
    QWidget::paintEvent(e);
}

void DockItem::mouseMoveEvent(QMouseEvent *e)
{
    QWidget::mouseMoveEvent(e);

    m_popupTipsDelayTimer->start();
}

void DockItem::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::RightButton)
        return showContextMenu();
}

void DockItem::enterEvent(QEvent *e)
{
    m_hover = true;
    m_popupTipsDelayTimer->start();

    update();

    return QWidget::enterEvent(e);
}

void DockItem::leaveEvent(QEvent *e)
{
    m_hover = false;
    m_popupTipsDelayTimer->stop();

    // auto hide if popup is not model window
    if (!PopupWindow->model())
    {
        m_popupShown = false;
        PopupWindow->hide();
    }

    update();

    return QWidget::leaveEvent(e);
}

const QRect DockItem::perfectIconRect() const
{
    const QRect itemRect = rect();
    const int iconSize = std::min(itemRect.width(), itemRect.height()) * 0.8;

    QRect iconRect;
    iconRect.setWidth(iconSize);
    iconRect.setHeight(iconSize);
    iconRect.moveTopLeft(itemRect.center() - iconRect.center());

    return iconRect;
}

void DockItem::showContextMenu()
{
    const QString menuJson = contextMenu();
    if (menuJson.isEmpty())
        return;

    QDBusPendingReply<QDBusObjectPath> result = m_menuManagerInter->RegisterMenu();

    result.waitForFinished();
    if (result.isError())
    {
        qWarning() << result.error();
        return;
    }

    const QPoint p = popupMarkPoint();

    QJsonObject menuObject;
    menuObject.insert("x", QJsonValue(p.x()));
    menuObject.insert("y", QJsonValue(p.y()));
    menuObject.insert("isDockMenu", QJsonValue(true));
    menuObject.insert("menuJsonContent", QJsonValue(menuJson));

    switch (DockPosition)
    {
    case Top:       menuObject.insert("direction", "top");      break;
    case Bottom:    menuObject.insert("direction", "bottom");   break;
    case Left:      menuObject.insert("direction", "left");     break;
    case Right:     menuObject.insert("direction", "right");    break;
    }

    const QDBusObjectPath path = result.argumentAt(0).value<QDBusObjectPath>();
    DBusMenu *menuInter = new DBusMenu(path.path(), this);

    connect(menuInter, &DBusMenu::ItemInvoked, this, &DockItem::invokedMenuItem);
    connect(menuInter, &DBusMenu::MenuUnregistered, this, &DockItem::requestRefershWindowVisible);
    connect(menuInter, &DBusMenu::MenuUnregistered, menuInter, &DBusMenu::deleteLater, Qt::QueuedConnection);

    menuInter->ShowMenu(QString(QJsonDocument(menuObject).toJson()));
}

void DockItem::showHoverTips()
{
    // another model popup window is alread exists
    if (PopupWindow->isVisible() && PopupWindow->model())
        return;

    QWidget * const content = popupTips();
    if (!content)
        return;

    showPopupWindow(content);
}

void DockItem::showPopupWindow(QWidget * const content, const bool model)
{
    m_popupShown = true;

    if (model)
        emit requestWindowAutoHide(false);

    DockPopupWindow *popup = PopupWindow.get();
    QWidget *lastContent = popup->getContent();
    if (lastContent)
        lastContent->hide();

    switch (DockPosition)
    {
    case Top:   popup->setArrowDirection(DockPopupWindow::ArrowTop);     break;
    case Bottom:popup->setArrowDirection(DockPopupWindow::ArrowBottom);  break;
    case Left:  popup->setArrowDirection(DockPopupWindow::ArrowLeft);    break;
    case Right: popup->setArrowDirection(DockPopupWindow::ArrowRight);   break;
    }
    popup->setContent(content);
    popup->setMargin(5);
    popup->setWidth(content->sizeHint().width());
    popup->setHeight(content->sizeHint().height());

    const QPoint p = popupMarkPoint();
    QMetaObject::invokeMethod(popup, "show", Qt::QueuedConnection, Q_ARG(QPoint, p), Q_ARG(bool, model));

    connect(popup, &DockPopupWindow::accept, this, &DockItem::popupWindowAccept);
}

void DockItem::popupWindowAccept()
{
    if (!PopupWindow->isVisible())
        return;

    disconnect(PopupWindow.get(), &DockPopupWindow::accept, this, &DockItem::popupWindowAccept);

    m_popupShown = false;
    PopupWindow->hide();

    emit requestWindowAutoHide(true);
}

void DockItem::showPopupApplet(QWidget * const applet)
{
    // another model popup window is alread exists
    if (PopupWindow->isVisible() && PopupWindow->model())
        return;

    showPopupWindow(applet, true);
}

void DockItem::invokedMenuItem(const QString &itemId, const bool checked)
{
    Q_UNUSED(itemId)
    Q_UNUSED(checked)
}

const QString DockItem::contextMenu() const
{
    return QString();
}

QWidget *DockItem::popupTips()
{
    return nullptr;
}

const QPoint DockItem::popupMarkPoint()
{
    QPoint p;
    QWidget *w = this;
    do {
        p += w->pos();
        w = qobject_cast<QWidget *>(w->parent());
    } while (w);

    const QRect r = rect();
    switch (DockPosition)
    {
    case Top:       p += QPoint(r.width() / 2, r.height());      break;
    case Bottom:    p += QPoint(r.width() / 2, 0);               break;
    case Left:      p += QPoint(r.width(), r.height() / 2);      break;
    case Right:     p += QPoint(0, r.height() / 2);              break;
    }

    return p;
}
