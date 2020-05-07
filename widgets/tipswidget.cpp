#include "tipswidget.h"

#include <QApplication>
#include <QPainter>

TipsWidget::TipsWidget(QWidget *parent) : QFrame(parent)
{
    connect(qApp, &QApplication::fontChanged, this, [=] {
         setText(m_text);
     });
}

void TipsWidget::setText(const QString &text)
{
    m_text = text;

    setFixedSize(fontMetrics().width(text) + 6, fontMetrics().height());

    update();
}

void TipsWidget::setTextList(const QStringList &textList)
{
    m_textList = textList;

    int maxLength = 0;
    int k = fontMetrics().height() * m_textList.size();
    setFixedHeight(k);
    for (QString text : m_textList) {
        int fontLength = fontMetrics().width(text) + 6;
        maxLength = maxLength > fontLength ? maxLength : fontLength;
    }
    m_width = maxLength;
    setFixedWidth(maxLength);

    update();
}

void TipsWidget::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);
    QPainter painter(this);
    painter.setPen(QPen(palette().brightText(), 1));
    QTextOption option;
    int fontHeight = fontMetrics().height();
    option.setAlignment(Qt::AlignCenter);

    if (!m_text.isEmpty() && m_textList.isEmpty()) {
        painter.drawText(rect(), m_text, option);
    }

    int y = 0;
    if (m_text.isEmpty() && !m_textList.isEmpty()) {
        if (m_textList.size() != 1)
            option.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        for (QString text : m_textList) {
            painter.drawText(QRect(0, y, m_width, fontHeight), text, option);
            y += fontHeight;
        }
    }

}
