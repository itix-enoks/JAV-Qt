#pragma once

#include <QWidget>

/**
 * Five stars to rate with, in halves: hovering fills them up to the half
 * star under the mouse, clicking sets that rating, clicking the current
 * rating again clears it to 0.
 */
class StarRating : public QWidget {
    Q_OBJECT

public:
    static constexpr int STARS = 5;

    StarRating(QWidget *parent = nullptr);

    double value() const { return m_value; }
    /* Does not emit valueChanged() */
    void setValue(double value);

    QSize sizeHint() const override;

signals:
    /* Only when changed by the user */
    void valueChanged(double value);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    static constexpr int STAR_SIZE = 26;
    static constexpr int GAP = 4;

    /* The rating a click at `x` gives: 0.5 to 5, or -1 left of the stars */
    double valueAt(qreal x) const;

    double m_value = 0;
    double m_hovered = -1;  /* -1: not hovering */
};
