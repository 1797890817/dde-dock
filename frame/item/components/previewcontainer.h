#ifndef PREVIEWCONTAINER_H
#define PREVIEWCONTAINER_H

#include <QWidget>
#include <QHBoxLayout>

#include "dbus/dbusdockentry.h"
#include "constants.h"

class PreviewContainer : public QWidget
{
    Q_OBJECT

public:
    explicit PreviewContainer(QWidget *parent = 0);

public:
    void setWindowInfos(const WindowDict &infos);

public slots:
    void updateLayoutDirection(const Dock::Position dockPos);

private:
    QBoxLayout *m_windowListLayout;
};

#endif // PREVIEWCONTAINER_H
