#ifndef APPSNAPSHOT_H
#define APPSNAPSHOT_H

#include <QWidget>
#include <QDebug>
#include <QTimer>

class AppSnapshot : public QWidget
{
    Q_OBJECT

public:
    explicit AppSnapshot(const WId wid, QWidget *parent = 0);

    WId wid() const { return m_wid; }
    const QImage snapshot() const { return m_snapshot; }
    const QString title() const { return m_title; }

signals:
    void entered(const WId wid) const;
    void clicked(const WId wid) const;

public slots:
    void fetchSnapshot();
    void closeWindow() const;
    void setWindowTitle(const QString &title);

private:
    void enterEvent(QEvent *e);
    void paintEvent(QPaintEvent *e);
    void mousePressEvent(QMouseEvent *e);

private:
    const WId m_wid;

    QString m_title;
    QImage m_snapshot;
};

#endif // APPSNAPSHOT_H
