/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     listenerri <listenerri@gmail.com>
 *
 * Maintainer: listenerri <listenerri@gmail.com>
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

#include "fashiontrayitem.h"
#include "system-trays/systemtrayitem.h"

#include <QDebug>
#include <QResizeEvent>

#define SpliterSize 2
#define TraySpace 10
#define TrayWidgetWidthMin 24
#define TrayWidgetHeightMin 24

#define ExpandedKey "fashion-tray-expanded"

int FashionTrayItem::TrayWidgetWidth = TrayWidgetWidthMin;
int FashionTrayItem::TrayWidgetHeight = TrayWidgetHeightMin;

FashionTrayItem::FashionTrayItem(TrayPlugin *trayPlugin, QWidget *parent)
    : QWidget(parent),
      m_mainBoxLayout(new QBoxLayout(QBoxLayout::Direction::LeftToRight)),
      m_trayBoxLayout(new QBoxLayout(QBoxLayout::Direction::LeftToRight)),
      m_leftSpliter(new QLabel),
      m_rightSpliter(new QLabel),
      m_attentionDelayTimer(new QTimer(this)),
      m_dockPosistion(trayPlugin->dockPosition()),
      m_trayPlugin(trayPlugin),
      m_controlWidget(new FashionTrayControlWidget(m_dockPosistion)),
      m_currentAttentionTray(nullptr),
      m_currentDraggingTray(nullptr)
{
    setAcceptDrops(true);

    m_leftSpliter->setStyleSheet("background-color: rgba(255, 255, 255, 0.1);");
    m_rightSpliter->setStyleSheet("background-color: rgba(255, 255, 255, 0.1);");

    m_controlWidget->setFixedSize(QSize(TrayWidgetWidth, TrayWidgetHeight));

    m_mainBoxLayout->setMargin(0);
    m_mainBoxLayout->setContentsMargins(0, 0, 0, 0);
    m_mainBoxLayout->setSpacing(TraySpace);

    m_trayBoxLayout->setMargin(0);
    m_trayBoxLayout->setContentsMargins(0, 0, 0, 0);
    m_trayBoxLayout->setSpacing(TraySpace);

    m_mainBoxLayout->addWidget(m_leftSpliter);
    m_mainBoxLayout->addLayout(m_trayBoxLayout);
    m_mainBoxLayout->addWidget(m_controlWidget);
    m_mainBoxLayout->addWidget(m_rightSpliter);

    m_mainBoxLayout->setAlignment(Qt::AlignCenter);
    m_trayBoxLayout->setAlignment(Qt::AlignCenter);
    m_mainBoxLayout->setAlignment(m_leftSpliter, Qt::AlignCenter);
    m_mainBoxLayout->setAlignment(m_controlWidget, Qt::AlignCenter);
    m_mainBoxLayout->setAlignment(m_rightSpliter, Qt::AlignCenter);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setLayout(m_mainBoxLayout);

    m_attentionDelayTimer->setInterval(3000);
    m_attentionDelayTimer->setSingleShot(true);

    connect(m_controlWidget, &FashionTrayControlWidget::expandChanged, this, &FashionTrayItem::onTrayListExpandChanged);

    // do not call init immediately the TrayPlugin has not be constructed for now
    QTimer::singleShot(0, this, &FashionTrayItem::init);
}

void FashionTrayItem::setTrayWidgets(const QMap<QString, AbstractTrayWidget *> &itemTrayMap)
{
    clearTrayWidgets();

    for (auto it = itemTrayMap.constBegin(); it != itemTrayMap.constEnd(); ++it) {
        trayWidgetAdded(it.key(), it.value());
    }
}

void FashionTrayItem::trayWidgetAdded(const QString &itemKey, AbstractTrayWidget *trayWidget)
{
    for (auto w : m_wrapperList) {
        if (w->absTrayWidget() == trayWidget) {
            qDebug() << "Reject! want to isert duplicate trayWidget:" << itemKey << trayWidget;
            return;
        }
    }

    FashionTrayWidgetWrapper *wrapper = new FashionTrayWidgetWrapper(itemKey, trayWidget);
    wrapper->setFixedSize(QSize(TrayWidgetWidth, TrayWidgetHeight));

    const int index = whereToInsert(wrapper);
    m_trayBoxLayout->insertWidget(index, wrapper);
    m_wrapperList.insert(index, wrapper);

    wrapper->setVisible(m_controlWidget->expanded());

    if (wrapper->attention()) {
        setCurrentAttentionTray(wrapper);
    }

    connect(wrapper, &FashionTrayWidgetWrapper::attentionChanged, this, &FashionTrayItem::onTrayAttentionChanged, static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection));
    connect(wrapper, &FashionTrayWidgetWrapper::dragStart, this, &FashionTrayItem::onItemDragStart, Qt::UniqueConnection);
    connect(wrapper, &FashionTrayWidgetWrapper::dragStop, this, &FashionTrayItem::onItemDragStop, Qt::UniqueConnection);
    connect(wrapper, &FashionTrayWidgetWrapper::requestSwapWithDragging, this, &FashionTrayItem::onItemRequestSwapWithDragging, Qt::UniqueConnection);

    requestResize();
}

void FashionTrayItem::trayWidgetRemoved(AbstractTrayWidget *trayWidget)
{
    bool founded = false;

    for (auto wrapper : m_wrapperList) {
        // found the removed tray
        if (wrapper->absTrayWidget() == trayWidget) {
            // the removed tray is a attention tray
            if (m_currentAttentionTray == wrapper) {
                if (m_controlWidget->expanded()) {
                    m_trayBoxLayout->removeWidget(m_currentAttentionTray);
                } else {
                    m_mainBoxLayout->removeWidget(m_currentAttentionTray);
                }
                m_currentAttentionTray = nullptr;
            } else {
                m_trayBoxLayout->removeWidget(wrapper);
            }
            // do not delete real tray object, just delete it's wrapper object
            // the real tray object should be deleted in TrayPlugin class
            trayWidget->setParent(nullptr);
            wrapper->deleteLater();
            m_wrapperList.removeAll(wrapper);
            founded = true;
            break;
        }
    }

    if (!founded) {
        qDebug() << "Error! can not find the tray widget in fashion tray list" << trayWidget;
    }

    requestResize();
}

void FashionTrayItem::clearTrayWidgets()
{
    QList<QPointer<FashionTrayWidgetWrapper>> mList = m_wrapperList;

    for (auto wrapper : mList) {
        trayWidgetRemoved(wrapper->absTrayWidget());
    }

    m_wrapperList.clear();

    requestResize();
}

void FashionTrayItem::setDockPostion(Dock::Position pos)
{
    m_dockPosistion = pos;

    m_controlWidget->setDockPostion(m_dockPosistion);
    SystemTrayItem::setDockPostion(m_dockPosistion);

    if (pos == Dock::Position::Top || pos == Dock::Position::Bottom) {
        m_mainBoxLayout->setDirection(QBoxLayout::Direction::LeftToRight);
        m_trayBoxLayout->setDirection(QBoxLayout::Direction::LeftToRight);
    } else{
        m_mainBoxLayout->setDirection(QBoxLayout::Direction::TopToBottom);
        m_trayBoxLayout->setDirection(QBoxLayout::Direction::TopToBottom);
    }

    requestResize();
}

void FashionTrayItem::onTrayListExpandChanged(const bool expand)
{
    m_trayPlugin->saveValue(ExpandedKey, expand);

    if (!isVisible())
        return;

    if (expand) {
        refreshTraysVisible();
    } else {
        // hide all tray immediately if Dock is in maxed size
        // the property "DockIsMaxiedSize" of qApp is set by DockSettings class
        if (qApp->property("DockIsMaxiedSize").toBool()) {
            refreshTraysVisible();
        } else {
            // hide all tray widget delay for fold animation
            QTimer::singleShot(350, this, [=] {refreshTraysVisible();});
        }
        requestResize();
    }
}

// used by QMetaObject::invokeMethod in TrayPluginItem / MainPanel class
void FashionTrayItem::setSuggestIconSize(QSize size)
{
    size = size * 0.6;

    int length = qMin(size.width(), size.height());
    // 设置最小值
//    length = qMax(length, TrayWidgetWidthMin);

    if (length == TrayWidgetWidth || length == TrayWidgetHeight) {
        return;
    }

    TrayWidgetWidth = length;
    TrayWidgetHeight = length;

    QSize newSize(length, length);

    m_controlWidget->setFixedSize(newSize);

    for (auto wrapper : m_wrapperList) {
        wrapper->setFixedSize(newSize);
    }

    requestResize();
}

void FashionTrayItem::setRightSplitVisible(const bool visible)
{
    if (visible) {
        m_rightSpliter->setStyleSheet("background-color: rgba(255, 255, 255, 0.1);");
    } else {
        m_rightSpliter->setStyleSheet("background-color: transparent;");
    }
}

void FashionTrayItem::showEvent(QShowEvent *event)
{
    requestResize();

    QWidget::showEvent(event);
}

void FashionTrayItem::hideEvent(QHideEvent *event)
{
    requestResize();

    QWidget::hideEvent(event);
}

void FashionTrayItem::resizeEvent(QResizeEvent *event)
{
    const QSize &mSize = event->size();

    if (m_dockPosistion == Dock::Position::Top || m_dockPosistion == Dock::Position::Bottom) {
        m_leftSpliter->setFixedSize(SpliterSize, mSize.height() * 0.8);
        m_rightSpliter->setFixedSize(SpliterSize, mSize.height() * 0.8);
    } else{
        m_leftSpliter->setFixedSize(mSize.width() * 0.8, SpliterSize);
        m_rightSpliter->setFixedSize(mSize.width() * 0.8, SpliterSize);
    }

    QWidget::resizeEvent(event);
}

void FashionTrayItem::dragEnterEvent(QDragEnterEvent *event)
{
    // accept but do not handle the trays drag event
    // in order to avoid the for forbidden label displayed on the mouse
    if (event->mimeData()->hasFormat(TRAY_ITEM_DRAG_MIMEDATA)) {
        event->accept();
        return;
    }

    QWidget::dragEnterEvent(event);
}

QSize FashionTrayItem::sizeHint() const
{
    return wantedTotalSize();
}

void FashionTrayItem::init()
{
    qDebug() << "init Fashion mode tray plugin item";
    m_controlWidget->setExpanded(m_trayPlugin->getValue(ExpandedKey, true).toBool());
    setDockPostion(m_dockPosistion);
    onTrayListExpandChanged(m_controlWidget->expanded());
}

QSize FashionTrayItem::wantedTotalSize() const
{
    QSize size;

    if (m_controlWidget->expanded()) {
        if (m_dockPosistion == Dock::Position::Top || m_dockPosistion == Dock::Position::Bottom) {
            size.setWidth(m_wrapperList.size() * TrayWidgetWidth // 所有插件
                          + TrayWidgetWidth // 控制按钮
                          + SpliterSize * 2 // 两个分隔条
                          + 3 * TraySpace // MainBoxLayout所有space
                          + (m_wrapperList.size() - 1) * TraySpace); // TrayBoxLayout所有space
            size.setHeight(height());
        } else {
            size.setWidth(width());
            size.setHeight(m_wrapperList.size() * TrayWidgetHeight // 所有插件
                          + TrayWidgetHeight // 控制按钮
                          + SpliterSize * 2 // 两个分隔条
                          + 3 * TraySpace // MainBoxLayout所有space
                          + (m_wrapperList.size() - 1) * TraySpace); // TrayBoxLayout所有space
        }
    } else {
        if (m_dockPosistion == Dock::Position::Top || m_dockPosistion == Dock::Position::Bottom) {
            size.setWidth(TrayWidgetWidth // 控制按钮
                          + (m_currentAttentionTray ? TrayWidgetWidth : 0) // 活动状态的tray
                          + SpliterSize * 2 // 两个分隔条
                          + 3 * TraySpace); // MainBoxLayout所有space
            size.setHeight(height());
        } else {
            size.setWidth(width());
            size.setHeight(TrayWidgetHeight // 控制按钮
                          + (m_currentAttentionTray ? TrayWidgetHeight : 0) // 活动状态的tray
                          + SpliterSize * 2 // 两个分隔条
                          + 3 * TraySpace); // MainBoxLayout所有space
        }
    }

    return size;
}

int FashionTrayItem::whereToInsert(FashionTrayWidgetWrapper *wrapper) const
{
    // 如果已经对图标进行过排序则完全按照从配置文件中获取的顺序来插入图标
    if (m_trayPlugin->traysSortedInFashionMode()) {
        return whereToInsertBySortKey(wrapper);
    }

    // 如果没有对图标进行过排序则使用下面的默认排序算法:
    // 所有应用图标在系统图标的左侧
    // 新的应用图标在最左侧的应用图标处插入
    // 新的系统图标在最左侧的系统图标处插入
    return whereToInsertByDefault(wrapper);
}

int FashionTrayItem::whereToInsertBySortKey(FashionTrayWidgetWrapper *wrapper) const
{
    if (m_wrapperList.isEmpty()) {
        return 0;
    }

    const int destSortKey = m_trayPlugin->itemSortKey(wrapper->itemKey());

    if (destSortKey < -1) {
        return 0;
    }
    if (destSortKey == -1) {
        return m_wrapperList.size();
    }

    // 当目标插入位置为列表的大小时将从最后面追加到列表中
    int destIndex = m_wrapperList.size();
    for (int i = 0; i < m_wrapperList.size(); ++i) {
        if (destSortKey > m_trayPlugin->itemSortKey(m_wrapperList.at(i)->itemKey())) {
            continue;
        }
        destIndex = i;
        break;
    }

    return destIndex;
}

int FashionTrayItem::whereToInsertByDefault(FashionTrayWidgetWrapper *wrapper) const
{
    int index = 0;
    switch (wrapper->absTrayWidget()->trayTyep()) {
    case AbstractTrayWidget::TrayType::ApplicationTray:
        index = whereToInsertAppTrayByDefault(wrapper);
        break;
    case AbstractTrayWidget::TrayType::SystemTray:
        index = whereToInsertSystemTrayByDefault(wrapper);
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    return index;
}

int FashionTrayItem::whereToInsertAppTrayByDefault(FashionTrayWidgetWrapper *wrapper) const
{
    if (m_wrapperList.isEmpty() || wrapper->absTrayWidget()->trayTyep() != AbstractTrayWidget::TrayType::ApplicationTray) {
        return 0;
    }

    int lastAppTrayIndex = -1;
    for (int i = 0; i < m_wrapperList.size(); ++i) {
        if (m_wrapperList.at(i)->absTrayWidget()->trayTyep() == AbstractTrayWidget::TrayType::ApplicationTray) {
            lastAppTrayIndex = i;
            continue;
        }
        break;
    }
    // there is no AppTray
    if (lastAppTrayIndex == -1) {
        return 0;
    }
    // the inserting tray is not a AppTray
    if (wrapper->absTrayWidget()->trayTyep() != AbstractTrayWidget::TrayType::ApplicationTray) {
        return lastAppTrayIndex + 1;
    }

    int insertIndex = m_trayPlugin->itemSortKey(wrapper->itemKey());
    // invalid index
    if (insertIndex < -1) {
        return 0;
    }
    for (int i = 0; i < m_wrapperList.size(); ++i) {
        if (m_wrapperList.at(i)->absTrayWidget()->trayTyep() != AbstractTrayWidget::TrayType::ApplicationTray) {
            insertIndex = i;
            break;
        }
        if (insertIndex > m_trayPlugin->itemSortKey(m_wrapperList.at(i)->itemKey())) {
            continue;
        }
        insertIndex = i;
        break;
    }
    if (insertIndex > lastAppTrayIndex + 1) {
        insertIndex = lastAppTrayIndex + 1;
    }

    return insertIndex;
}

int FashionTrayItem::whereToInsertSystemTrayByDefault(FashionTrayWidgetWrapper *wrapper) const
{
    if (m_wrapperList.isEmpty()) {
        return 0;
    }

    int firstSystemTrayIndex = -1;
    for (int i = 0; i < m_wrapperList.size(); ++i) {
        if (m_wrapperList.at(i)->absTrayWidget()->trayTyep() == AbstractTrayWidget::TrayType::SystemTray) {
            firstSystemTrayIndex = i;
            break;
        }
    }
    // there is no SystemTray
    if (firstSystemTrayIndex == -1) {
        return m_wrapperList.size();
    }
    // the inserting tray is not a SystemTray
    if (wrapper->absTrayWidget()->trayTyep() != AbstractTrayWidget::TrayType::SystemTray) {
        return firstSystemTrayIndex;
    }

    int insertIndex = m_trayPlugin->itemSortKey(wrapper->itemKey());
    // invalid index
    if (insertIndex < -1) {
        return firstSystemTrayIndex;
    }
    for (int i = 0; i < m_wrapperList.size(); ++i) {
        if (m_wrapperList.at(i)->absTrayWidget()->trayTyep() != AbstractTrayWidget::TrayType::SystemTray) {
            continue;
        }
        if (insertIndex > m_trayPlugin->itemSortKey(m_wrapperList.at(i)->itemKey())) {
            continue;
        }
        insertIndex = i;
        break;
    }
    if (insertIndex < firstSystemTrayIndex) {
        return firstSystemTrayIndex;
    }

    return insertIndex;
}

void FashionTrayItem::saveCurrentOrderToConfig()
{
    for (int i = 0; i < m_wrapperList.size(); ++i) {
        m_trayPlugin->setSortKey(m_wrapperList.at(i)->itemKey(), i + 1);
    }
}

void FashionTrayItem::onTrayAttentionChanged(const bool attention)
{
    // 设置attention为false之后，启动timer，在timer处于Active状态期间不重设attention为true
    if (!attention) {
        m_attentionDelayTimer->start();
    } else if (attention && m_attentionDelayTimer->isActive()) {
        return;
    }

    FashionTrayWidgetWrapper *wrapper = static_cast<FashionTrayWidgetWrapper *>(sender());

    Q_ASSERT(wrapper);

    if (attention) {
        setCurrentAttentionTray(wrapper);
    } else {
        if (m_currentAttentionTray != wrapper) {
            return;
        }

        if (m_controlWidget->expanded()) {
            m_currentAttentionTray = nullptr;
        } else {
            moveInAttionTray();
            m_currentAttentionTray = nullptr;
            requestResize();
        }
    }
}

void FashionTrayItem::setCurrentAttentionTray(FashionTrayWidgetWrapper *attentionWrapper)
{
    if (!attentionWrapper) {
        return;
    }

    if (m_controlWidget->expanded()) {
        m_currentAttentionTray = attentionWrapper;
    } else {
        if (m_currentAttentionTray == attentionWrapper) {
            return;
        }
        moveInAttionTray();
        bool sizeChanged = !m_currentAttentionTray;
        m_currentAttentionTray = attentionWrapper;
        moveOutAttionTray();
        if (sizeChanged) {
            requestResize();
        }
    }

    m_mainBoxLayout->setAlignment(m_currentAttentionTray, Qt::AlignCenter);
}

void FashionTrayItem::requestResize()
{
    // reset property "FashionTraySize" to notify dock resize
    // DockPluginsController will watch this property
    setProperty("FashionTraySize", sizeHint());
}

void FashionTrayItem::moveOutAttionTray()
{
    if (!m_currentAttentionTray) {
        return;
    }

    m_trayBoxLayout->removeWidget(m_currentAttentionTray);
    m_mainBoxLayout->insertWidget(m_mainBoxLayout->indexOf(m_rightSpliter), m_currentAttentionTray);
    m_currentAttentionTray->setVisible(true);
}

void FashionTrayItem::moveInAttionTray()
{
    if (!m_currentAttentionTray) {
        return;
    }

    m_mainBoxLayout->removeWidget(m_currentAttentionTray);
    m_trayBoxLayout->insertWidget(whereToInsert(m_currentAttentionTray), m_currentAttentionTray);
    m_currentAttentionTray->setVisible(false);
    m_currentAttentionTray->setAttention(false);
}

void FashionTrayItem::switchAttionTray(FashionTrayWidgetWrapper *attentionWrapper)
{
    if (!m_currentAttentionTray || !attentionWrapper) {
        return;
    }

    m_mainBoxLayout->replaceWidget(m_currentAttentionTray, attentionWrapper);
    m_trayBoxLayout->removeWidget(attentionWrapper);
    m_trayBoxLayout->insertWidget(whereToInsert(m_currentAttentionTray), m_currentAttentionTray);

    attentionWrapper->setVisible(true);
    m_currentAttentionTray->setVisible(m_controlWidget->expanded());

    m_currentAttentionTray = attentionWrapper;
}

void FashionTrayItem::refreshTraysVisible()
{
    const bool expand = m_controlWidget->expanded();

    if (m_currentAttentionTray) {
        if (expand) {
            m_mainBoxLayout->removeWidget(m_currentAttentionTray);
            m_trayBoxLayout->insertWidget(whereToInsert(m_currentAttentionTray), m_currentAttentionTray);
        }

        m_currentAttentionTray = nullptr;
    }

    for (auto wrapper : m_wrapperList) {
        wrapper->setVisible(expand);
        // reset all tray item attention state
        wrapper->setAttention(false);
    }

    m_attentionDelayTimer->start();

    requestResize();
}

void FashionTrayItem::onItemDragStart()
{
    FashionTrayWidgetWrapper *wrapper = static_cast<FashionTrayWidgetWrapper *>(sender());

    if (!wrapper) {
        return;
    }

    m_currentDraggingTray = wrapper;
}

void FashionTrayItem::onItemDragStop()
{
    FashionTrayWidgetWrapper *wrapper = static_cast<FashionTrayWidgetWrapper *>(sender());

    if (!wrapper) {
        return;
    }

    if (m_currentDraggingTray == wrapper) {
        m_currentDraggingTray = nullptr;
    } else {
        Q_UNREACHABLE();
    }

    saveCurrentOrderToConfig();
}

void FashionTrayItem::onItemRequestSwapWithDragging()
{
    FashionTrayWidgetWrapper *wrapper = static_cast<FashionTrayWidgetWrapper *>(sender());

    if (!wrapper || !m_currentDraggingTray || wrapper == m_currentDraggingTray) {
        return;
    }

    const int indexOfDest = m_trayBoxLayout->indexOf(wrapper);
    const int indexOfDragging = m_trayBoxLayout->indexOf(m_currentDraggingTray);

    m_trayBoxLayout->removeWidget(m_currentDraggingTray);
    m_trayBoxLayout->insertWidget(indexOfDest, m_currentDraggingTray);

    m_wrapperList.insert(indexOfDest, m_wrapperList.takeAt(indexOfDragging));
}
