#include "gui/starrating.hpp"
#include "gui/style.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {

    /* A five-pointed star filling `box`, point up */
    QPainterPath starPath(const QRectF &box)
    {
        const QPointF center = box.center() + QPointF(0, box.height() * 0.05);
        const qreal outer = box.width() / 2, inner = outer * 0.45;
        QPainterPath path;
        for (int i = 0; i < 10; ++i) {
            const qreal radius = i % 2 ? inner : outer;
            const qreal angle = -std::numbers::pi / 2 + i * std::numbers::pi / 5;
            const QPointF point = center + QPointF(radius * std::cos(angle), radius * std::sin(angle));
            i == 0 ? path.moveTo(point) : path.lineTo(point);
        }
        path.closeSubpath();
        return path;
    }

} /* namespace */

StarRating::StarRating(QWidget *parent) : QWidget(parent)
{
    this->setMouseTracking(true);
    this->setCursor(Qt::PointingHandCursor);
    this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    this->setToolTip(tr("Click a star, or its left half for a half star. Click your rating again to clear it."));
}

QSize StarRating::sizeHint() const
{
    return {STARS * STAR_SIZE + (STARS - 1) * GAP + 2, STAR_SIZE + 2};
}

void StarRating::setValue(double value)
{
    m_value = std::clamp(std::round(value * 2) / 2, 0.0, double(STARS));
    this->update();
}

double StarRating::valueAt(qreal x) const
{
    if (x < 0)
        return -1;
    const int star = std::min(int(x / (STAR_SIZE + GAP)), STARS - 1);
    const qreal within = x - star * (STAR_SIZE + GAP);
    return star + (within < STAR_SIZE / 2.0 ? 0.5 : 1.0);
}

void StarRating::mouseMoveEvent(QMouseEvent *event)
{
    const double hovered = this->valueAt(event->position().x() - 1);
    if (hovered != m_hovered) {
        m_hovered = hovered;
        this->update();
    }
}

void StarRating::leaveEvent(QEvent *)
{
    m_hovered = -1;
    this->update();
}

void StarRating::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const double clicked = this->valueAt(event->position().x() - 1);
    if (clicked < 0)
        return;
    m_value = clicked == m_value ? 0 : clicked;
    this->update();
    emit this->valueChanged(m_value);
}

void StarRating::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const bool hovering = m_hovered >= 0;
    const double shown = hovering ? m_hovered : m_value;
    const QColor fill = hovering ? style::theme().starHover : style::theme().star;
    const QColor outline = style::theme().postBorder.darker(140);

    for (int i = 0; i < STARS; ++i) {
        const QRectF box(1 + i * (STAR_SIZE + GAP), 1, STAR_SIZE, STAR_SIZE);
        const QPainterPath star = starPath(box);
        painter.fillPath(star, style::theme().blank);

        /* Fully, half or not filled */
        const double amount = std::clamp(shown - i, 0.0, 1.0);
        if (amount > 0) {
            painter.save();
            painter.setClipRect(QRectF(box.left(), box.top(), box.width() * amount, box.height()));
            painter.fillPath(star, fill);
            painter.restore();
        }
        painter.setPen(QPen(amount > 0 ? fill.darker(115) : outline, 1.2));
        painter.drawPath(star);
    }
}
