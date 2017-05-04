#include "previewwidget.h"

#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xutil.h>

#include <QX11Info>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>

#define PREVIEW_W 200
#define PREVIEW_H 130
#define PREVIEW_M 12

PreviewWidget::PreviewWidget(const WId wid, QWidget *parent)
    : QWidget(parent),

      m_wid(wid),
      m_mouseEnterTimer(new QTimer(this)),

      m_hovered(false)
{
    m_closeButton = new DImageButton;
    m_closeButton->setFixedSize(24, 24);
    m_closeButton->setVisible(false);
    m_closeButton->setNormalPic(":/icons/resources/close_round_normal.png");
    m_closeButton->setHoverPic(":/icons/resources/close_round_hover.png");
    m_closeButton->setPressPic(":/icons/resources/close_round_press.png");

    m_droppedDelay = new QTimer(this);
    m_droppedDelay->setSingleShot(true);
    m_droppedDelay->setInterval(100);

    m_mouseEnterTimer->setInterval(200);
    m_mouseEnterTimer->setSingleShot(true);

    QVBoxLayout *centralLayout = new QVBoxLayout;
    centralLayout->setSpacing(0);
    centralLayout->setMargin(0);
    centralLayout->addWidget(m_closeButton);
    centralLayout->setAlignment(m_closeButton, Qt::AlignTop | Qt::AlignRight);

    setFixedSize(PREVIEW_W + PREVIEW_M * 2, PREVIEW_H + PREVIEW_M * 2);
    setLayout(centralLayout);
    setAcceptDrops(true);

    connect(m_closeButton, &DImageButton::clicked, this, &PreviewWidget::closeWindow);
    connect(m_mouseEnterTimer, &QTimer::timeout, this, &PreviewWidget::showPreview, Qt::QueuedConnection);

    QTimer::singleShot(1, this, &PreviewWidget::refreshImage);
}

void PreviewWidget::setTitle(const QString &title)
{
    m_title = title;

    update();
}

void PreviewWidget::refreshImage()
{
    const auto display = QX11Info::display();

    XWindowAttributes attrs;
    XGetWindowAttributes(display, m_wid, &attrs);
    XImage *ximage = XGetImage(display, m_wid, 0, 0, attrs.width, attrs.height, AllPlanes, ZPixmap);

    const QImage qimage((const uchar*)(ximage->data), ximage->width, ximage->height, ximage->bytes_per_line, QImage::Format_RGB32);
    m_image = qimage.scaled(PREVIEW_W, PREVIEW_H, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    m_image = m_image.copy((m_image.width() - PREVIEW_W) / 2, (m_image.height() - PREVIEW_H) / 2, PREVIEW_W, PREVIEW_H);
    XDestroyImage(ximage);

    update();
}

void PreviewWidget::closeWindow()
{
    const auto display = QX11Info::display();

    XEvent e;

    memset(&e, 0, sizeof(e));
    e.xclient.type = ClientMessage;
    e.xclient.window = m_wid;
    e.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", true);
    e.xclient.format = 32;
    e.xclient.data.l[0] = XInternAtom(display, "WM_DELETE_WINDOW", false);
    e.xclient.data.l[1] = CurrentTime;

    XSendEvent(display, m_wid, false, NoEventMask, &e);
//    XDestroyWindow(display, m_wid);
    XFlush(display);
}

void PreviewWidget::showPreview()
{
    emit requestPreviewWindow(m_wid);
}

void PreviewWidget::paintEvent(QPaintEvent *e)
{
    const QRect r = rect().marginsRemoved(QMargins(PREVIEW_M, PREVIEW_M, PREVIEW_M, PREVIEW_M));

    QPainter painter(this);

    // draw image
    const QRect ir = m_image.rect();
    const QPoint offset = r.center() - ir.center();

    painter.fillRect(offset.x(), offset.y(), ir.width(), ir.height(), Qt::white);
    painter.drawImage(offset.x(), offset.y(), m_image);

    // bottom black background
    QRect bgr = r;
    bgr.setTop(bgr.bottom() - 30);
    painter.fillRect(bgr, QColor(0, 0, 0, 255 * 0.3));
    // bottom title
    painter.drawText(bgr, Qt::AlignCenter, m_title);

    // draw border
    if (m_hovered)
    {
        const QRect br = r.marginsAdded(QMargins(1, 1, 1, 1));
        QPen p;
        p.setBrush(Qt::white);
        p.setWidth(4);
        painter.setBrush(Qt::transparent);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.drawRoundedRect(br, 3, 3);
    }

    QWidget::paintEvent(e);
}

void PreviewWidget::enterEvent(QEvent *e)
{
    if (m_droppedDelay->isActive())
        return e->ignore();

    m_hovered = true;
    m_closeButton->setVisible(true);
    m_mouseEnterTimer->start();

    update();

    QWidget::enterEvent(e);
}

void PreviewWidget::leaveEvent(QEvent *e)
{
    m_hovered = false;
    m_closeButton->setVisible(false);
    m_mouseEnterTimer->stop();

    update();

    QWidget::leaveEvent(e);
}

void PreviewWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_droppedDelay->isActive())
        return e->ignore();

    QWidget::mouseReleaseEvent(e);

    m_mouseEnterTimer->stop();
    emit requestHidePreview();
    emit requestCancelPreview();
    emit requestActivateWindow(m_wid);
}

void PreviewWidget::dragEnterEvent(QDragEnterEvent *e)
{
    e->accept();

    m_hovered = true;

    update();

    emit requestActivateWindow(m_wid);
}

void PreviewWidget::dragLeaveEvent(QDragLeaveEvent *e)
{
    QWidget::dragLeaveEvent(e);

    m_hovered = false;

    update();
}

void PreviewWidget::dropEvent(QDropEvent *e)
{
    m_droppedDelay->start();

    QWidget::dropEvent(e);

    emit requestCancelPreview();
}
