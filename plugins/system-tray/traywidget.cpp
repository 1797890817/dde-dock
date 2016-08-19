#include "traywidget.h"

#include <QWindow>
#include <QPainter>
#include <QX11Info>
#include <QDebug>
#include <QMouseEvent>

#include <X11/extensions/shape.h>
#include <X11/Xregion.h>

#include <xcb/composite.h>
#include <xcb/xcb_image.h>

static const quint16 iconSize = 16;

#define DRAG_THRESHOLD  20

void sni_cleanup_xcb_image(void *data)
{
    xcb_image_destroy(static_cast<xcb_image_t*>(data));
}

TrayWidget::TrayWidget(quint32 winId, QWidget *parent)
    : QWidget(parent),
      m_windowId(winId)
{
    wrapWindow();
    updateIcon();

    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(500);
    m_updateTimer->setSingleShot(false);
    m_updateTimer->start();

    connect(m_updateTimer, &QTimer::timeout, this, &TrayWidget::updateIcon);

    setFixedSize(26, 26);
}

TrayWidget::~TrayWidget()
{
}

const QImage TrayWidget::trayImage() const
{
    return getImageNonComposite();
}

QSize TrayWidget::sizeHint() const
{
    return QSize(26, 26);
}

void TrayWidget::showEvent(QShowEvent *e)
{
    m_image = getImageNonComposite();

    QWidget::showEvent(e);

    setX11PassMouseEvent(false);

//    auto c = QX11Info::connection();
//    QPoint globalPos = mapToGlobal(QPoint(0, 0));
//    const uint32_t windowMoveConfigVals[2] = { uint32_t(globalPos.x()), uint32_t(globalPos.y()) };
//    xcb_configure_window(c, m_containerWid,
//                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
//                         windowMoveConfigVals);
}

void TrayWidget::hideEvent(QHideEvent *e)
{
    QWidget::hideEvent(e);

    setX11PassMouseEvent(true);
}

void TrayWidget::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    const QPoint p(width() / 2 - iconSize / 2, height() / 2 - iconSize / 2);

    QPainter painter;
    painter.begin(this);
    painter.setRenderHint(QPainter::Antialiasing);

//    m_image = getImageNonComposite();
//    if (!m_image.isNull()) {
//        if (true) {
//            QPainterPath path;
//            path.addRoundedRect(p.x(), p.y(), iconSize, iconSize, iconSize / 2, iconSize / 2);

//            painter.setClipPath(path);
//        }

//        painter.drawImage(p.x(), p.y(), m_image.scaled(iconSize, iconSize));
//    }
    painter.drawImage(rect().center() - m_image.rect().center(), m_image);

    painter.end();
}

void TrayWidget::mousePressEvent(QMouseEvent *e)
{
    const QPoint point(e->pos() - rect().center());
    if (point.manhattanLength() > 24)
        return QWidget::mousePressEvent(e);

    m_pressPoint = e->pos();
}

void TrayWidget::mouseReleaseEvent(QMouseEvent *e)
{
    QWidget::mouseReleaseEvent(e);

    const QPoint point(e->pos() - rect().center());
    if (point.manhattanLength() > 24)
        return;

    const QPoint distance = e->pos() - m_pressPoint;
    if (distance.manhattanLength() > DRAG_THRESHOLD)
        return;

    QPoint globalPos = mapToGlobal(QPoint(0, 0));
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
}

void TrayWidget::wrapWindow()
{
    auto c = QX11Info::connection();

    auto cookie = xcb_get_geometry(c, m_windowId);
    QScopedPointer<xcb_get_geometry_reply_t> clientGeom(xcb_get_geometry_reply(c, cookie, Q_NULLPTR));
    if (clientGeom.isNull())
        return;

    //create a container window
    auto screen = xcb_setup_roots_iterator (xcb_get_setup (c)).data;
    m_containerWid = xcb_generate_id(c);
    uint32_t values[2];
    auto mask = XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT;
    values[0] = screen->black_pixel; //draw a solid background so the embeded icon doesn't get garbage in it
    values[1] = true; //bypass wM
    xcb_create_window (c,                          /* connection    */
                       XCB_COPY_FROM_PARENT,          /* depth         */
                       m_containerWid,               /* window Id     */
                       screen->root,                 /* parent window */
                       0, 0,                         /* x, y          */
                       iconSize, iconSize,     /* width, height */
                       0,                            /* border_width  */
                       XCB_WINDOW_CLASS_INPUT_OUTPUT,/* class         */
                       screen->root_visual,          /* visual        */
                       mask, values);                /* masks         */

    /*
        We need the window to exist and be mapped otherwise the child won't render it's contents

        We also need it to exist in the right place to get the clicks working as GTK will check sendEvent locations to see if our window is in the right place. So even though our contents are drawn via compositing we still put this window in the right place

        We can't composite it away anything parented owned by the root window (apparently)
        Stack Under works in the non composited case, but it doesn't seem to work in kwin's composited case (probably need set relevant NETWM hint)

        As a last resort set opacity to 0 just to make sure this container never appears
    */
//    const uint32_t stackBelowData[] = {XCB_STACK_MODE_BELOW};
//    xcb_configure_window(c, m_containerWid, XCB_CONFIG_WINDOW_STACK_MODE, stackBelowData);

    QWindow * win = QWindow::fromWinId(m_containerWid);
    win->setOpacity(0);

    setX11PassMouseEvent(true);

    xcb_flush(c);

    xcb_map_window(c, m_containerWid);

    xcb_reparent_window(c, m_windowId,
                        m_containerWid,
                        0, 0);

    /*
     * Render the embedded window offscreen
     */
    xcb_composite_redirect_window(c, m_windowId, XCB_COMPOSITE_REDIRECT_MANUAL);


    /* we grab the window, but also make sure it's automatically reparented back
     * to the root window if we should die.
    */
    xcb_change_save_set(c, XCB_SET_MODE_INSERT, m_windowId);

    //tell client we're embedding it
    // xembed_message_send(m_windowId, XEMBED_EMBEDDED_NOTIFY, m_containerWid, 0, 0);

    //move window we're embedding
    /*
    const uint32_t windowMoveConfigVals[2] = { 0, 0 };
    xcb_configure_window(c, m_windowId,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                         windowMoveCentially quitting the application. Returns onfigVals);
    */

    //if the window is a clearly stupid size resize to be something sensible
    //this is needed as chormium and such when resized just fill the icon with transparent space and only draw in the middle
    //however spotify does need this as by default the window size is 900px wide.
    //use an artbitrary heuristic to make sure icons are always sensible
    if (clientGeom->width > iconSize || clientGeom->height > iconSize )
    {
        const uint32_t windowMoveConfigVals[2] = { iconSize, iconSize };
        xcb_configure_window(c, m_windowId,
                             XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                             windowMoveConfigVals);
    }

    //show the embedded window otherwise nothing happens
    xcb_map_window(c, m_windowId);

    xcb_clear_area(c, 0, m_windowId, 0, 0, qMin(clientGeom->width, iconSize), qMin(clientGeom->height, iconSize));

    xcb_flush(c);
}

void TrayWidget::updateIcon()
{
    if (!isVisible()) return;

//    auto c = QX11Info::connection();

//    const uint32_t stackAboveData[] = {XCB_STACK_MODE_ABOVE};
//    xcb_configure_window(c, m_containerWid, XCB_CONFIG_WINDOW_STACK_MODE, stackAboveData);

//    QPoint globalPos = mapToGlobal(QPoint(0, 0));
//    const uint32_t windowMoveConfigVals[2] = { uint32_t(globalPos.x()), uint32_t(globalPos.y()) };
//    xcb_configure_window(c, m_containerWid,
//                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
//                         windowMoveConfigVals);

//    const uint32_t windowResizeConfigVals[2] = { iconSize, iconSize };
//    xcb_configure_window(c, m_windowId,
//                         XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
//                         windowResizeConfigVals);

    m_image = getImageNonComposite();
    update();
}

//void TrayWidget::hideIcon()
//{
//    auto c = QX11Info::connection();

//    const uint32_t stackAboveData[] = {XCB_STACK_MODE_BELOW};
//    xcb_configure_window(c, m_containerWid, XCB_CONFIG_WINDOW_STACK_MODE, stackAboveData);

//    const uint32_t windowMoveConfigVals[2] = {0, 0};
//    xcb_configure_window(c, m_containerWid,
//                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
//                         windowMoveConfigVals);

//    hide();
//}

void TrayWidget::sendClick(uint8_t mouseButton, int x, int y)
{
    //it's best not to look at this code
    //GTK doesn't like send_events and double checks the mouse position matches where the window is and is top level
    //in order to solve this we move the embed container over to where the mouse is then replay the event using send_event
    //if patching, test with xchat + xchat context menus

    //note x,y are not actually where the mouse is, but the plasmoid
    //ideally we should make this match the plasmoid hit area

    auto c = QX11Info::connection();

    auto cookieSize = xcb_get_geometry(c, m_windowId);
    QScopedPointer<xcb_get_geometry_reply_t> clientGeom(xcb_get_geometry_reply(c, cookieSize, Q_NULLPTR));

    auto cookie = xcb_query_pointer(c, m_windowId);
    QScopedPointer<xcb_query_pointer_reply_t> pointer(xcb_query_pointer_reply(c, cookie, Q_NULLPTR));

//    qDebug() << pointer->root_x << pointer->root_y << x << y << clientGeom->width << clientGeom->height;

    //move our window so the mouse is within its geometry
    uint32_t configVals[2] = {0, 0};
    if (mouseButton >= XCB_BUTTON_INDEX_4) {
        //scroll event, take pointer position
        configVals[0] = pointer->root_x;
        configVals[1] = pointer->root_y;
    } else {
        if (pointer->root_x > x + clientGeom->width)
            configVals[0] = pointer->root_x - clientGeom->width + 1;
        else
            configVals[0] = static_cast<uint32_t>(x);
        if (pointer->root_y > y + clientGeom->height)
            configVals[1] = pointer->root_y - clientGeom->height + 1;
        else
            configVals[1] = static_cast<uint32_t>(y);
    }
    xcb_configure_window(c, m_containerWid, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, configVals);

    //pull window up
//    const uint32_t stackAboveData[] = {XCB_STACK_MODE_ABOVE};
//    xcb_configure_window(c, m_containerWid, XCB_CONFIG_WINDOW_STACK_MODE, stackAboveData);

//    system(QString("xdotool click --window %1 %2").arg(m_windowId).arg(mouseButton).toLatin1());

    //mouse down
    {
        xcb_button_press_event_t* event = new xcb_button_press_event_t;
        memset(event, 0x00, sizeof(xcb_button_press_event_t));
        event->response_type = XCB_BUTTON_PRESS;
        event->event = m_windowId;
        event->time = QX11Info::getTimestamp();
        event->same_screen = 1;
        event->root = QX11Info::appRootWindow();
        event->root_x = x;
        event->root_y = y;
        event->event_x = 0;
        event->event_y = 0;
        event->child = 0;
        event->state = 0;
        event->detail = mouseButton;

        xcb_send_event(c, false, m_windowId, XCB_EVENT_MASK_BUTTON_PRESS, (char *) event);
//        free(event);
        delete event;
    }

    //mouse up
    {
        xcb_button_release_event_t* event = new xcb_button_release_event_t;
        memset(event, 0x00, sizeof(xcb_button_release_event_t));
        event->response_type = XCB_BUTTON_RELEASE;
        event->event = m_windowId;
        event->time = QX11Info::getTimestamp();
        event->same_screen = 1;
        event->root = QX11Info::appRootWindow();
        event->root_x = x;
        event->root_y = y;
        event->event_x = 0;
        event->event_y = 0;
        event->child = 0;
        event->state = 0;
        event->detail = mouseButton;

        xcb_send_event(c, false, m_windowId, XCB_EVENT_MASK_BUTTON_RELEASE, (char *) event);
//        free(event);
        delete event;
    }

//    const uint32_t stackBelowData[] = {XCB_STACK_MODE_BELOW};
//    xcb_configure_window(c, m_containerWid, XCB_CONFIG_WINDOW_STACK_MODE, stackBelowData);
}

QImage TrayWidget::getImageNonComposite() const
{
    auto c = QX11Info::connection();
    auto cookie = xcb_get_geometry(c, m_windowId);
    QScopedPointer<xcb_get_geometry_reply_t> geom(xcb_get_geometry_reply(c, cookie, Q_NULLPTR));
    if (geom.isNull())
        return QImage();

    xcb_image_t *image = xcb_image_get(c, m_windowId, 0, 0, geom->width, geom->height, ~0, XCB_IMAGE_FORMAT_Z_PIXMAP);

    if (image == NULL)
        return QImage();

    QImage qimage(image->data, image->width, image->height, image->stride, QImage::Format_ARGB32, sni_cleanup_xcb_image, image);

    return qimage;
}

void TrayWidget::setX11PassMouseEvent(const bool pass)
{
    auto c = QX11Info::connection();

    if (pass)
    {
        XShapeCombineRectangles(QX11Info::display(), m_containerWid, ShapeInput, 0, 0, nullptr, 0, ShapeSet, YXBanded);

        const uint32_t stackAboveData[] = {XCB_STACK_MODE_BELOW};
        xcb_configure_window(c, m_containerWid, XCB_CONFIG_WINDOW_STACK_MODE, stackAboveData);
    }
    else
    {
        XRectangle rectangle;
        rectangle.x = 0;
        rectangle.y = 0;
        rectangle.width = iconSize;
        rectangle.height = iconSize;

        XShapeCombineRectangles(QX11Info::display(), m_containerWid, ShapeInput, 0, 0, &rectangle, 1, ShapeSet, YXBanded);

        const uint32_t stackAboveData[] = {XCB_STACK_MODE_ABOVE};
        xcb_configure_window(c, m_containerWid, XCB_CONFIG_WINDOW_STACK_MODE, stackAboveData);
    }
}
