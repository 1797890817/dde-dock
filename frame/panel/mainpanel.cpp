#include "mainpanel.h"
#include "item/appitem.h"

#include <QBoxLayout>
#include <QDragEnterEvent>

DockItem *MainPanel::DragingItem = nullptr;
PlaceholderItem *MainPanel::RequestDockItem = nullptr;

const char *RequestDockKey = "RequestDock";

MainPanel::MainPanel(QWidget *parent)
    : QFrame(parent),
      m_position(Dock::Top),
      m_displayMode(Dock::Fashion),
      m_itemLayout(new QBoxLayout(QBoxLayout::LeftToRight)),

      m_itemAdjustTimer(new QTimer(this)),
      m_itemController(DockItemController::instance(this))
{
    m_itemLayout->setSpacing(0);
    m_itemLayout->setContentsMargins(0, 0, 0, 0);

    setAcceptDrops(true);
    setAccessibleName("dock-mainpanel");
    setObjectName("MainPanel");
    setStyleSheet("QWidget #MainPanel {"
                  "border:" xstr(PANEL_BORDER) "px solid rgba(162, 162, 162, .2);"
                  "background-color:rgba(10, 10, 10, .6);"
                  "}"
                  "QWidget #MainPanel[displayMode='1'] {"
                  "border:none;"
                  "}"
                  // Top
                  "QWidget #MainPanel[displayMode='0'][position='0'] {"
                  "border-bottom-left-radius:5px;"
                  "border-bottom-right-radius:5px;"
                  "}"
                  // Right
                  "QWidget #MainPanel[displayMode='0'][position='1'] {"
                  "border-top-left-radius:5px;"
                  "border-bottom-left-radius:5px;"
                  "}"
                  // Bottom
                  "QWidget #MainPanel[displayMode='0'][position='2'] {"
                  "border-top-left-radius:6px;"
                  "border-top-right-radius:6px;"
                  "}"
                  // Left
                  "QWidget #MainPanel[displayMode='0'][position='3'] {"
                  "border-top-right-radius:5px;"
                  "border-bottom-right-radius:5px;"
                  "}"
                  "QWidget #MainPanel[position='0'] {"
                  "padding:0 " xstr(PANEL_PADDING) "px;"
                  "border-top:none;"
                  "}"
                  "QWidget #MainPanel[position='1'] {"
                  "padding:" xstr(PANEL_PADDING) "px 0;"
                  "border-right:none;"
                  "}"
                  "QWidget #MainPanel[position='2'] {"
                  "padding:0 " xstr(PANEL_PADDING) "px;"
                  "border-bottom:none;"
                  "}"
                  "QWidget #MainPanel[position='3'] {"
                  "padding:" xstr(PANEL_PADDING) "px 0;"
                  "border-left:none;"
                  "}");

    connect(m_itemController, &DockItemController::itemInserted, this, &MainPanel::itemInserted, Qt::DirectConnection);
    connect(m_itemController, &DockItemController::itemRemoved, this, &MainPanel::itemRemoved, Qt::DirectConnection);
    connect(m_itemController, &DockItemController::itemMoved, this, &MainPanel::itemMoved);
    connect(m_itemController, &DockItemController::itemManaged, this, &MainPanel::manageItem);
    connect(m_itemAdjustTimer, &QTimer::timeout, this, &MainPanel::adjustItemSize, Qt::QueuedConnection);

    m_itemAdjustTimer->setSingleShot(true);
    m_itemAdjustTimer->setInterval(100);

    const QList<DockItem *> itemList = m_itemController->itemList();
    for (auto item : itemList)
    {
        manageItem(item);
        m_itemLayout->addWidget(item);
    }

    setLayout(m_itemLayout);
}

void MainPanel::updateDockPosition(const Position dockPosition)
{
    m_position = dockPosition;

    switch (m_position)
    {
    case Position::Top:
    case Position::Bottom:          m_itemLayout->setDirection(QBoxLayout::LeftToRight);    break;
    case Position::Left:
    case Position::Right:           m_itemLayout->setDirection(QBoxLayout::TopToBottom);    break;
    }

    m_itemAdjustTimer->start();
}

void MainPanel::updateDockDisplayMode(const DisplayMode displayMode)
{
    m_displayMode = displayMode;

//    const QList<DockItem *> itemList = m_itemController->itemList();
//    for (auto item : itemList)
//    {
//        if (item->itemType() == DockItem::Container)
//            item->setVisible(displayMode == Dock::Efficient);
//    }

    // reload qss
    setStyleSheet(styleSheet());
}

int MainPanel::displayMode()
{
    return int(m_displayMode);
}

int MainPanel::position()
{
    return int(m_position);
}

void MainPanel::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);

    m_itemAdjustTimer->start();
}

void MainPanel::dragEnterEvent(QDragEnterEvent *e)
{
    DockItem *item = itemAt(e->pos());
    if (item && item->itemType() == DockItem::Container)
        return;

    DockItem *dragSourceItem = qobject_cast<DockItem *>(e->source());
    if (dragSourceItem)
    {
        e->accept();
        if (DragingItem)
            DragingItem->show();
        return;
    }

    if (!e->mimeData()->formats().contains(RequestDockKey))
        return;
    if (m_itemController->appIsOnDock(e->mimeData()->data(RequestDockKey)))
        return;

    e->accept();
}

void MainPanel::dragMoveEvent(QDragMoveEvent *e)
{
    e->accept();

    DockItem *dst = itemAt(e->pos());
    if (!dst)
        return;

    // internal drag swap
    if (e->source())
    {
        if (dst == DragingItem)
            return;
        if (!DragingItem)
            return;
        if (m_itemController->itemIsInContainer(DragingItem))
            return;

        m_itemController->itemMove(DragingItem, dst);
    } else {
        if (!RequestDockItem)
        {
            DockItem *insertPositionItem = itemAt(e->pos());
            if (!insertPositionItem || insertPositionItem->itemType() != DockItem::App)
                return;
            RequestDockItem = new PlaceholderItem;
            m_itemController->placeholderItemAdded(RequestDockItem, insertPositionItem);
        } else {
            if (dst == RequestDockItem)
                return;

            m_itemController->itemMove(RequestDockItem, dst);
        }
    }
}

void MainPanel::dragLeaveEvent(QDragLeaveEvent *e)
{
    Q_UNUSED(e)

    if (RequestDockItem)
    {
        const QRect r(static_cast<QWidget *>(parent())->pos(), size());
        const QPoint p(QCursor::pos());

        if (r.contains(p))
            return;

        m_itemController->placeholderItemRemoved(RequestDockItem);
        RequestDockItem->deleteLater();
        RequestDockItem = nullptr;
    }

    if (DragingItem && DragingItem->itemType() != DockItem::Plugins)
        DragingItem->hide();
}

void MainPanel::dropEvent(QDropEvent *e)
{
    Q_UNUSED(e)

    DragingItem = nullptr;

    if (RequestDockItem)
    {
        m_itemController->placeholderItemDocked(e->mimeData()->data(RequestDockKey), RequestDockItem);
        m_itemController->placeholderItemRemoved(RequestDockItem);
        RequestDockItem->deleteLater();
        RequestDockItem = nullptr;
    }
}

void MainPanel::manageItem(DockItem *item)
{
    connect(item, &DockItem::dragStarted, this, &MainPanel::itemDragStarted, Qt::UniqueConnection);
    connect(item, &DockItem::itemDropped, this, &MainPanel::itemDropped, Qt::UniqueConnection);
    connect(item, &DockItem::requestRefershWindowVisible, this, &MainPanel::requestRefershWindowVisible, Qt::UniqueConnection);
    connect(item, &DockItem::requestWindowAutoHide, this, &MainPanel::requestWindowAutoHide, Qt::UniqueConnection);
}

DockItem *MainPanel::itemAt(const QPoint &point)
{
    const QList<DockItem *> itemList = m_itemController->itemList();

    for (auto item : itemList)
    {
        QRect rect;
        rect.setTopLeft(item->pos());
        rect.setSize(item->size());

        if (rect.contains(point))
            return item;
    }

    return nullptr;
}

void MainPanel::adjustItemSize()
{
    Q_ASSERT(sender() == m_itemAdjustTimer);

    QSize itemSize;
    switch (m_position)
    {
    case Top:
    case Bottom:
        itemSize.setHeight(height() - PANEL_BORDER);
        itemSize.setWidth(AppItem::itemBaseWidth());
        break;

    case Left:
    case Right:
        itemSize.setHeight(AppItem::itemBaseHeight());
        itemSize.setWidth(width() - PANEL_BORDER);
        break;

    default:
        Q_ASSERT(false);
    }

    if (itemSize.height() < 0 || itemSize.width() < 0)
        return;

    int totalAppItemCount = 0;
    int totalWidth = 0;
    int totalHeight = 0;
    const QList<DockItem *> itemList = m_itemController->itemList();
    for (auto item : itemList)
    {
        if (item->itemType() == DockItem::Container)
            continue;
        if (m_itemController->itemIsInContainer(item))
            continue;

        QMetaObject::invokeMethod(item, "setVisible", Qt::QueuedConnection, Q_ARG(bool, true));

        switch (item->itemType())
        {
        case DockItem::App:
        case DockItem::Launcher:
            item->setFixedSize(itemSize);
            ++totalAppItemCount;
            totalWidth += itemSize.width();
            totalHeight += itemSize.height();
            break;
        case DockItem::Plugins:
            if (m_displayMode == Fashion)
            {
                item->setFixedSize(itemSize);
                ++totalAppItemCount;
                totalWidth += itemSize.width();
                totalHeight += itemSize.height();
            }
            else
            {
                const QSize size = item->sizeHint();
                item->setFixedSize(size);
                if (m_position == Dock::Top || m_position == Dock::Bottom)
                    item->setFixedHeight(itemSize.height());
                else
                    item->setFixedWidth(itemSize.width());
                totalWidth += size.width();
                totalHeight += size.height();
            }
            break;
        default:;
        }
    }

    const int w = width() - PANEL_BORDER * 2 - PANEL_PADDING * 2;
    const int h = height() - PANEL_BORDER * 2 - PANEL_PADDING * 2;

    // test if panel can display all items completely
    bool containsCompletely = false;
    switch (m_position)
    {
    case Dock::Top:
    case Dock::Bottom:
        containsCompletely = totalWidth <= w;     break;

    case Dock::Left:
    case Dock::Right:
        containsCompletely = totalHeight <= h;   break;

    default:
        Q_ASSERT(false);
    }

    // abort adjust.
    if (containsCompletely)
        return;

    // now, we need to decrease item size to fit panel size
    int overflow;
    int base;
    if (m_position == Dock::Top || m_position == Dock::Bottom)
    {
//        qDebug() << "width: " << totalWidth << width();
        overflow = totalWidth;
        base = w;
    }
    else
    {
//        qDebug() << "height: " << totalHeight << height();
        overflow = totalHeight;
        base = h;
    }

    const int decrease = double(overflow - base) / totalAppItemCount;
    int extraDecrease = overflow - base - decrease * totalAppItemCount;

    for (auto item : itemList)
    {
        const DockItem::ItemType itemType = item->itemType();
        if (itemType == DockItem::Stretch || itemType == DockItem::Container)
            continue;
        if (itemType == DockItem::Plugins)
            if (m_displayMode != Dock::Fashion)
                continue;
        if (m_itemController->itemIsInContainer(item))
            continue;

        switch (m_position)
        {
        case Dock::Top:
        case Dock::Bottom:
            item->setFixedWidth(item->width() - decrease - bool(extraDecrease));
            break;

        case Dock::Left:
        case Dock::Right:
            item->setFixedHeight(item->height() - decrease - bool(extraDecrease));
            break;
        }

        if (extraDecrease)
            --extraDecrease;
    }

    // ensure all extra space assigned
    Q_ASSERT(extraDecrease == 0);

    update();
}

void MainPanel::itemInserted(const int index, DockItem *item)
{
    // hide new item, display it after size adjust finished
    item->hide();

    manageItem(item);
    m_itemLayout->insertWidget(index, item);

    m_itemAdjustTimer->start();
}

void MainPanel::itemRemoved(DockItem *item)
{
    m_itemLayout->removeWidget(item);

    m_itemAdjustTimer->start();
}

void MainPanel::itemMoved(DockItem *item, const int index)
{
    // remove old item
    m_itemLayout->removeWidget(item);
    // insert new position
    m_itemLayout->insertWidget(index, item);
}

void MainPanel::itemDragStarted()
{
    DragingItem = qobject_cast<DockItem *>(sender());

    QRect rect;
    rect.setTopLeft(mapToGlobal(pos()));
    rect.setSize(size());

    DragingItem->setVisible(rect.contains(QCursor::pos()));
}

void MainPanel::itemDropped(QObject *destnation)
{
    if (m_displayMode == Dock::Fashion)
        return;

    DockItem *src = qobject_cast<DockItem *>(sender());
//    DockItem *dst = qobject_cast<DockItem *>(destnation);

    if (!src)
        return;

    const bool itemIsInContainer = m_itemController->itemIsInContainer(src);

    // drag from container
    if (itemIsInContainer && src->itemType() == DockItem::Plugins && destnation == this)
        m_itemController->itemDragOutFromContainer(src);

    // drop to container
    if (!itemIsInContainer && src->parent() == this && destnation != this)
        m_itemController->itemDroppedIntoContainer(src);

    m_itemAdjustTimer->start();
}
