#include "wirelessapplet.h"
#include "accesspointwidget.h"

#include <QJsonDocument>
#include <QScreen>
#include <QScreen>
#include <QGuiApplication>

#include <dinputdialog.h>
#include <dcheckbox.h>

DWIDGET_USE_NAMESPACE

#define WIDTH           300
#define MAX_HEIGHT      300
#define ITEM_HEIGHT     30

WirelessApplet::WirelessApplet(const QSet<NetworkDevice>::const_iterator &deviceIter, QWidget *parent)
    : QScrollArea(parent),

      m_device(*deviceIter),
      m_activeAP(),

      m_updateAPTimer(new QTimer(this)),

      m_centeralLayout(new QVBoxLayout),
      m_centeralWidget(new QWidget),
      m_controlPanel(new DeviceControlWidget(this)),
      m_networkInter(new DBusNetwork(this))
{
    setFixedHeight(WIDTH);

    m_updateAPTimer->setSingleShot(true);
    m_updateAPTimer->setInterval(100);

    m_centeralWidget->setFixedWidth(WIDTH);
    m_centeralWidget->setLayout(m_centeralLayout);

    m_centeralLayout->addWidget(m_controlPanel);
    m_centeralLayout->setSpacing(0);
    m_centeralLayout->setMargin(0);

    setWidget(m_centeralWidget);
    setFrameStyle(QFrame::NoFrame);
    setFixedWidth(300);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setStyleSheet("background-color:transparent;");

    QMetaObject::invokeMethod(this, "init", Qt::QueuedConnection);

    connect(m_networkInter, &DBusNetwork::AccessPointAdded, this, &WirelessApplet::APAdded);
    connect(m_networkInter, &DBusNetwork::AccessPointRemoved, this, &WirelessApplet::APRemoved);
    connect(m_networkInter, &DBusNetwork::AccessPointPropertiesChanged, this, &WirelessApplet::APPropertiesChanged);
    connect(m_networkInter, &DBusNetwork::DevicesChanged, this, &WirelessApplet::deviceStateChanegd);
    connect(m_networkInter, &DBusNetwork::NeedSecrets, this, &WirelessApplet::needSecrets);

    connect(m_controlPanel, &DeviceControlWidget::deviceEnableChanged, this, &WirelessApplet::deviceEnableChanged);

    connect(m_updateAPTimer, &QTimer::timeout, this, &WirelessApplet::updateAPList);

    connect(this, &WirelessApplet::activeAPChanged, m_updateAPTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
}

NetworkDevice::NetworkState WirelessApplet::wirelessState() const
{
    return m_device.state();
}

int WirelessApplet::activeAPStrgength() const
{
    return m_activeAP.strength();
}

void WirelessApplet::init()
{
    setDeviceInfo();
    loadAPList();
    onActiveAPChanged();
}

void WirelessApplet::APAdded(const QString &devPath, const QString &info)
{
    if (devPath != m_device.path())
        return;

    AccessPoint ap(info);
    if (m_apList.contains(ap))
        return;

    m_apList.append(ap);
    m_updateAPTimer->start();
}

void WirelessApplet::APRemoved(const QString &devPath, const QString &info)
{
    if (devPath != m_device.path())
        return;

    m_apList.removeOne(AccessPoint(info));
    m_updateAPTimer->start();
}

void WirelessApplet::setDeviceInfo()
{
    // set device enable state
    m_controlPanel->setDeviceEnabled(m_networkInter->IsDeviceEnabled(m_device.dbusPath()));
    m_controlPanel->setDeviceName(m_device.vendor());
}

void WirelessApplet::loadAPList()
{
    const QString data = m_networkInter->GetAccessPoints(m_device.dbusPath());
    const QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    Q_ASSERT(doc.isArray());

    for (auto item : doc.array())
    {
        Q_ASSERT(item.isObject());

        AccessPoint ap(item.toObject());
        if (!m_apList.contains(ap))
            m_apList.append(ap);
    }

    m_updateAPTimer->start();
}

void WirelessApplet::APPropertiesChanged(const QString &devPath, const QString &info)
{
    if (devPath != m_device.path())
        return;

    QJsonDocument doc = QJsonDocument::fromJson(info.toUtf8());
    Q_ASSERT(doc.isObject());
    const AccessPoint ap(doc.object());

    auto it = std::find_if(m_apList.begin(), m_apList.end(),
                           [&] (const AccessPoint &a) {return a == ap;});

    if (it == m_apList.end())
        return;

    *it = ap;
    if (m_activeAP.path() == ap.path())
    {
        m_activeAP = ap;
        emit activeAPChanged();
    }

//    if (*it > ap)
//    {
//        *it = ap;
//        m_activeAP = ap;
//        m_updateAPTimer->start();

//        emit activeAPChanged();
//    }
}

void WirelessApplet::updateAPList()
{
    Q_ASSERT(sender() == m_updateAPTimer);

    // remove old items
    while (QLayoutItem *item = m_centeralLayout->takeAt(1))
    {
        delete item->widget();
        delete item;
    }

    int avaliableAPCount = 0;

    if (m_networkInter->IsDeviceEnabled(m_device.dbusPath()))
    {
        // sort ap list by strength
        std::sort(m_apList.begin(), m_apList.end(), std::greater<AccessPoint>());

        for (auto ap : m_apList)
        {
            AccessPointWidget *apw = new AccessPointWidget(ap);
            apw->setFixedHeight(ITEM_HEIGHT);
            if (ap == m_activeAP)
                apw->setActive(true);

            connect(apw, &AccessPointWidget::requestActiveAP, this, &WirelessApplet::activateAP);

            m_centeralLayout->addWidget(apw);

            ++avaliableAPCount;
        }
    }
    m_controlPanel->setSeperatorVisible(avaliableAPCount);

    const int contentHeight = avaliableAPCount * ITEM_HEIGHT + m_controlPanel->height();
    m_centeralWidget->setFixedHeight(contentHeight);
    setFixedHeight(std::min(contentHeight, MAX_HEIGHT));
}

void WirelessApplet::deviceEnableChanged(const bool enable)
{
    m_networkInter->EnableDevice(m_device.dbusPath(), enable);
    m_updateAPTimer->start();
}

void WirelessApplet::deviceStateChanegd()
{
    const QJsonDocument doc = QJsonDocument::fromJson(m_networkInter->devices().toUtf8());
    Q_ASSERT(doc.isObject());
    const QJsonObject obj = doc.object();

    for (auto infoList(obj.constBegin()); infoList != obj.constEnd(); ++infoList)
    {
        Q_ASSERT(infoList.value().isArray());

        if (infoList.key() != "wireless")
            continue;

        for (auto wireless : infoList.value().toArray())
        {
            const QJsonObject info = wireless.toObject();
            if (info.value("Path") == m_device.path())
            {
                const NetworkDevice prevInfo = m_device;
                m_device = NetworkDevice(NetworkDevice::Wireless, info);

                if (prevInfo.state() != m_device.state())
                    emit wirelessStateChanged(m_device.state());
                if (prevInfo.activeAp() != m_device.activeAp())
                    onActiveAPChanged();

                break;
            }
        }
    }

}

void WirelessApplet::onActiveAPChanged()
{
    const QJsonDocument doc = QJsonDocument::fromJson(m_networkInter->GetAccessPoints(m_device.dbusPath()).value().toUtf8());
    Q_ASSERT(doc.isArray());

    for (auto dev : doc.array())
    {
        Q_ASSERT(dev.isObject());
        const QJsonObject obj = dev.toObject();

        if (obj.value("Path").toString() != m_device.activeAp())
            continue;

        m_activeAP = AccessPoint(obj);
        break;
    }

    emit activeAPChanged();
}

void WirelessApplet::activateAP(const QDBusObjectPath &apPath, const QString &ssid)
{
    QString uuid;

    const QJsonDocument doc = QJsonDocument::fromJson(m_networkInter->connections().toUtf8());
    for (auto it : doc.object().value("wireless").toArray())
    {
        const QJsonObject obj = it.toObject();
        if (obj.value("Ssid").toString() != ssid)
            continue;
        if (obj.value("HwAddress").toString() != m_device.hwAddress())
            continue;

        uuid = obj.value("Uuid").toString();
        if (!uuid.isEmpty())
            break;
    }

    m_networkInter->ActivateAccessPoint(uuid, apPath, m_device.dbusPath()).waitForFinished();
}

void WirelessApplet::needSecrets(const QString &apPath, const QString &uuid, const QString &ssid, const bool defaultAutoConnect)
{
    qDebug() << apPath << uuid << ssid << defaultAutoConnect;

    DInputDialog *dialog = new DInputDialog(this);
    DCheckBox *checkBox = new DCheckBox(dialog);

    checkBox->setChecked(defaultAutoConnect);
    checkBox->setText(tr("Auto-connect"));

    dialog->setTextEchoMode(DLineEdit::Password);
    dialog->setIcon(QIcon::fromTheme("notification-network-wireless-full"));
    dialog->addSpacing(10);
    dialog->addContent(checkBox, Qt::AlignLeft);
    dialog->setOkButtonText(tr("Connect"));

    connect(m_networkInter, &DBusNetwork::NeedSecretsFinished, dialog, &DInputDialog::close);
    connect(dialog, &DInputDialog::closed, dialog, &DInputDialog::deleteLater, Qt::QueuedConnection);
    connect(dialog, &DInputDialog::textValueChanged, [=] {dialog->setTextAlert(false);});
    connect(dialog, &DInputDialog::cancelButtonClicked, [=] {
        m_networkInter->CancelSecret(apPath, uuid);
        dialog->close();
    });
    connect(dialog, &DInputDialog::okButtonClicked, [=] {
        if (dialog->textValue().isEmpty())
            return dialog->setTextAlert(true);
        m_networkInter->FeedSecret(apPath, uuid, dialog->textValue(), checkBox->isChecked());
    });

    dialog->setTitle(tr("Password required to connect to <font color=\"#faca57\">%1</font>").arg(ssid));
    dialog->open();
    dialog->raise();

    QTimer::singleShot(500, [dialog] {
        dialog->move(qApp->primaryScreen()->geometry().center() - dialog->rect().center());
    });
}
