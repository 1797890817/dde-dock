/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#include "mainwidget.h"
#include "xcb_misc.h"
#include "controller/stylemanager.h"
#include "dbus/dbuspanelmanager.h"

#include <QApplication>

// Keep it longer than dock show/hide animation duration.
const int UPDATE_STRUT_PARTIAL_DELAY = 350;

const int ENTER_DELAY_INTERVAL = 200;
MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent),
      m_dockProperty(new DBusPanelManager(this))
{
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    this->setAttribute(Qt::WA_TranslucentBackground);
    //the attribute "Qt::WA_X11DoNotAcceptFocus" will not tack effect, not know the reason
    //this->setAttribute(Qt::WA_X11DoNotAcceptFocus);

    initHideStateManager();

    m_mainPanel = new DockPanel(this);
    connect(m_mainPanel, &DockPanel::startShow, this, &MainWidget::showDock);
    connect(m_mainPanel, &DockPanel::panelHasHidden, this, &MainWidget::hideDock);
    connect(m_mainPanel, &DockPanel::sizeChanged, this, &MainWidget::onPanelSizeChanged, Qt::QueuedConnection);

    connect(m_dmd, &DockModeData::dockModeChanged, this, &MainWidget::onDockModeChanged);
    connect(m_dmd, &DockModeData::hideModeChanged, this, &MainWidget::onHideModeChanged);

    //For init
    m_display = new DBusDisplay(this);
    m_windowStayRect = m_display->primaryRect();
//    updateXcbStrutPartial();

    m_positionUpdateTimer = new QTimer(this);
    m_positionUpdateTimer->setSingleShot(true);
    m_positionUpdateTimer->setInterval(500);

    DockUIDbus *dockUIDbus = new DockUIDbus(this);
    Q_UNUSED(dockUIDbus)

    XcbMisc::instance()->set_window_type(winId(), XcbMisc::Dock);

    connect(m_display, &DBusDisplay::PrimaryRectChanged, this, &MainWidget::updateGeometry);
    connect(m_display, &DBusDisplay::ScreenHeightChanged, this, &MainWidget::updateGeometry);
    connect(m_display, &DBusDisplay::ScreenWidthChanged, this, &MainWidget::updateGeometry);
    connect(m_positionUpdateTimer, &QTimer::timeout, this, &MainWidget::updatePosition, Qt::QueuedConnection);

    connect(m_mainPanel, &DockPanel::pluginsInitDone, this, &MainWidget::show);

    m_positionUpdateTimer->start();
//    onPanelSizeChanged();
}

void MainWidget::onDockModeChanged()
{
    // force update position twice
    m_positionUpdateTimer->start();
    updateGeometry();
    updateBackendProperty();
}

void MainWidget::onHideModeChanged()
{
    updateGeometry();
}

// TODO: it should be named to `updateSize' instead I think.
void MainWidget::updatePosition()
{
    const QRect rec = m_windowStayRect;

    qDebug() << "update position with screen rect: " << rec;

//    clearXcbStrutPartial();

    const int ww = m_dmd->getDockMode() == Dock::FashionMode ? width() : rec.width();
    const int hh = height();

    // pass invalid data
    if (!ww || !hh || hh > 100)
        return;

    qDebug() << "w: " << ww << ", h: " << hh;

    const Dock::DockMode dockMode = m_dmd->getDockMode();
    const int w = dockMode == Dock::FashionMode ? m_mainPanel->sizeHint().width() : rec.width();
    if (dockMode != Dock::FashionMode)
//        m_mainPanel->setFixedWidth(w);
        m_mainPanel->onContentsSizeChanged();

    if (m_hasHidden) {
        //set height with 0 mean window is hidden,Windows manager will handle it's showing animation
        this->setFixedSize(w, 1);

        m_windowStayPoint = QPoint(rec.x() + (rec.width() - ww) / 2,
                                    rec.y() + rec.height() - 1);
//        this->move//1 pixel for grab mouse enter event to show panel
    } else {
        this->setFixedSize(w, m_dmd->getDockHeight());

        m_windowStayPoint = QPoint(rec.x() + (rec.width() - ww) / 2,
                                    rec.y() + rec.height() - hh/* - 10*/);
    }

    this->move(m_windowStayPoint.x(), m_windowStayPoint.y());

    if (dockMode == Dock::FashionMode)
        updateBackendProperty();
    updateXcbStrutPartial();
//    QTimer::singleShot(1000, this, &MainWidget::updateXcbStrutPartial);
}

void MainWidget::updateXcbStrutPartial()
{
    clearXcbStrutPartial();

    int tmpHeight = 0;
    DBusDockSetting dds;
    if (dds.GetHideMode() == Dock::KeepShowing) {
        // qApp's screenHeight is wrong. its a bug, use dbus data instead.
//        int maxMonitorHeight = qApp->desktop()->size().height();
//        int max = 0;
//        for (QScreen *screen : qApp->screens())
//        {
//            QRect screenRect = screen->geometry();
//            max = qMax(max, screenRect.y() + screenRect.height());
//        }

//        qDebug() << "max = " << max;

        int maxMonitorHeight = m_display->screenHeight();
        tmpHeight = maxMonitorHeight - m_windowStayPoint.y();
    }

    // sometimes screen height is wrong, we need to ignore wrong data.
    if ((tmpHeight && tmpHeight < m_dmd->getDockHeight()) || tmpHeight > 100) {
        return;
    }

    // Set the strut partial to be full-width of the primary screen to
    // avoid some strange bugs.
    const QRect primaryRect = m_windowStayRect;

    XcbMisc::instance()->set_strut_partial(winId(),
                                           XcbMisc::OrientationBottom,
                                           tmpHeight,
                                           primaryRect.x(),
                                           primaryRect.x() + primaryRect.width());
// The line below causes deepin-wm to regard dde-dock as a normal window
// while previewing windows. https://github.com/fasheng/arch-deepin/issues/249
    //    this->setVisible(true);
}

void MainWidget::clearXcbStrutPartial()
{
    XcbMisc::instance()->set_strut_partial(winId(),
                                           XcbMisc::OrientationBottom,
                                           0, 0, 0);
}

void MainWidget::updateBackendProperty()
{
    m_dockProperty->SetPanelWidth(width());
}

void MainWidget::updateGeometry()
{
    QRect primaryRect = m_display->primaryRect();

//    for (const QScreen *screen : qApp->screens()) {
//        if (screen->name() == m_display->primary()) {
//            primaryRect = screen->geometry();
//            connect(screen, &QScreen::geometryChanged, this, &MainWidget::updateGeometry, Qt::UniqueConnection);
//        } else {
//            disconnect(screen, &QScreen::geometryChanged, this, &MainWidget::updateGeometry);
//        }
//    }

    m_windowStayRect = primaryRect;
    m_positionUpdateTimer->start();
}

void MainWidget::move(const int ax, const int ay)
{
//    QWidget::move(ax, ay);
    if (ax < 0 || ay < 0)
        return;
    XcbMisc::instance()->set_window_position(winId(), QPoint(ax, ay));

//    qDebug() << "move to " << ax << ',' << ay;
}

void MainWidget::initHideStateManager()
{
    m_dhsm = new DBusHideStateManager(this);
    m_dhsm->SetState(Dock::HideStateHiding);
}

void MainWidget::enterEvent(QEvent *)
{
    if (height() == 1) {
        QTimer *st = new QTimer(this);
        connect(st, &QTimer::timeout, this, [ = ] {
            //make sure the panel will show by mouse-enter
            if (geometry().contains(QCursor::pos()))
            {
                qDebug() << "MouseEntered, show dock...";
                emit m_mainPanel->startShow();
            }
            sender()->deleteLater();
        });
        st->start(ENTER_DELAY_INTERVAL);
    }
}

void MainWidget::leaveEvent(QEvent *)
{
    if (!this->geometry().contains(QCursor::pos())) {
        m_dhsm->UpdateState();
    }
}

void MainWidget::showDock()
{
    m_hasHidden = false;
    m_positionUpdateTimer->start();
}

void MainWidget::hideDock()
{
    m_hasHidden = true;
    m_positionUpdateTimer->start();
}

void MainWidget::onPanelSizeChanged()
{
//    m_windowRectDelayApplyTimer->start();

    if (m_dmd->getDockMode() != Dock::FashionMode)
        return;

    const QRect rec = m_windowStayRect;

    const int ww = m_dmd->getDockMode() == Dock::FashionMode ? width() : rec.width();
    const int hh = height();

    // pass invalid data
    if (!ww || !hh || hh > 100)
        return;

    qDebug() << "onPanelSizeChanged " << rec;
    qDebug() << "w: " << ww << ", h: " << hh;

    const int w = m_mainPanel->sizeHint().width();

    if (m_hasHidden) {
        //set height with 0 mean window is hidden,Windows manager will handle it's showing animation
        this->setFixedSize(w, 1);

        this->move(rec.x() + (rec.width() - ww) / 2,
                   rec.y() + rec.height() - 1);//1 pixel for grab mouse enter event to show panel
    } else {
        this->setFixedSize(w, m_dmd->getDockHeight());

        move(rec.x() + (rec.width() - ww) / 2,
             rec.y() + rec.height() - hh /*- 10*/);
    }
//        setFixedWidth(m_mainPanel->sizeHint().width());
//        updatePosition();
//                updateGeometry();
}

MainWidget::~MainWidget()
{
    qDebug() << "dde-dock destroyed";
}

void MainWidget::loadResources()
{
    m_mainPanel->loadResources();
}

DockUIDbus::DockUIDbus(MainWidget *parent):
    QDBusAbstractAdaptor(parent),
    m_parent(parent)
{
    QDBusConnection::sessionBus().registerObject(DBUS_PATH, parent);
}

DockUIDbus::~DockUIDbus()
{

}

qulonglong DockUIDbus::Xid()
{
    return m_parent->winId();
}

QString DockUIDbus::currentStyleName()
{
    return StyleManager::instance()->currentStyle();
}

QStringList DockUIDbus::styleNameList()
{
    return StyleManager::instance()->styleNameList();
}

void DockUIDbus::applyStyle(const QString &styleName)
{
    StyleManager::instance()->applyStyle(styleName);
}
