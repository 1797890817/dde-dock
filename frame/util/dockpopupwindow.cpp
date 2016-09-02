#include "dockpopupwindow.h"

#include <QScreen>
#include <QApplication>
#include <QDesktopWidget>

DWIDGET_USE_NAMESPACE

#define MOUSE_BUTTON    1 << 1

DockPopupWindow::DockPopupWindow(QWidget *parent)
    : DArrowRectangle(ArrowBottom, parent),
      m_model(false),

      m_acceptDelayTimer(new QTimer(this)),

      m_mouseInter(new DBusXMouseArea(this)),
      m_displayInter(new DBusDisplay(this))
{
    m_acceptDelayTimer->setSingleShot(true);
    m_acceptDelayTimer->setInterval(100);

    setWindowFlags(Qt::X11BypassWindowManagerHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_InputMethodEnabled, false);
    setFocusPolicy(Qt::StrongFocus);

    connect(m_acceptDelayTimer, &QTimer::timeout, this, &DockPopupWindow::accept);
}

DockPopupWindow::~DockPopupWindow()
{
}

bool DockPopupWindow::model() const
{
    return m_model;
}

void DockPopupWindow::setContent(QWidget *content)
{
    QWidget *lastWidget = getContent();
    if (lastWidget)
        lastWidget->removeEventFilter(this);
    content->installEventFilter(this);

    setAccessibleName(content->objectName() + "-popup");

    DArrowRectangle::setContent(content);
}

void DockPopupWindow::show(const QPoint &pos, const bool model)
{
    m_model = model;
    m_lastPoint = pos;

    DArrowRectangle::show(pos.x(), pos.y());

    if (!model && !m_mouseAreaKey.isEmpty())
        unRegisterMouseEvent();

    if (model && m_mouseAreaKey.isEmpty())
        registerMouseEvent();
}

void DockPopupWindow::hide()
{
    if (!m_mouseAreaKey.isEmpty())
        unRegisterMouseEvent();

    DArrowRectangle::hide();
}

void DockPopupWindow::enterEvent(QEvent *e)
{
    DArrowRectangle::enterEvent(e);

    raise();
    setFocus(Qt::ActiveWindowFocusReason);
}

void DockPopupWindow::mousePressEvent(QMouseEvent *e)
{
    DArrowRectangle::mousePressEvent(e);

//    if (e->button() == Qt::LeftButton)
//        m_acceptDelayTimer->start();
}

bool DockPopupWindow::eventFilter(QObject *o, QEvent *e)
{
    if (o != getContent() || e->type() != QEvent::Resize)
        return false;

    // FIXME: ensure position move after global mouse release event
    QTimer::singleShot(100, this, [this] {if (isVisible()) show(m_lastPoint, m_model);});

    return false;
}

void DockPopupWindow::globalMouseRelease(int button, int x, int y, const QString &id)
{
    Q_UNUSED(button);

    if (id != m_mouseAreaKey)
        return;

    Q_ASSERT(m_model);

    const QRect rect = QRect(pos(), size());
    const QPoint pos = QPoint(x, y);

    if (rect.contains(pos))
        return;

    emit accept();

    unRegisterMouseEvent();
}

void DockPopupWindow::registerMouseEvent()
{
    // only regist mouse button event
    m_mouseAreaKey = m_mouseInter->RegisterArea(0, 0, m_displayInter->screenWidth(), m_displayInter->screenHeight(), MOUSE_BUTTON);
//    m_mouseAreaKey = m_mouseInter->RegisterFullScreen();

    connect(m_mouseInter, &DBusXMouseArea::ButtonRelease, this, &DockPopupWindow::globalMouseRelease, Qt::UniqueConnection);
}

void DockPopupWindow::unRegisterMouseEvent()
{
    if (m_mouseAreaKey.isEmpty())
        return;

    disconnect(m_mouseInter, &DBusXMouseArea::ButtonRelease, this, &DockPopupWindow::globalMouseRelease);

    m_mouseInter->UnregisterArea(m_mouseAreaKey);
    m_mouseAreaKey.clear();
}
