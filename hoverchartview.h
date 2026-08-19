#ifndef HOVERCHARTVIEW_H
#define HOVERCHARTVIEW_H

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#ifndef QT_CHARTS_USE_NAMESPACE
#define QT_CHARTS_USE_NAMESPACE
#endif
QT_CHARTS_USE_NAMESPACE

#include <QLabel>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QEvent>
#include <QLocale>
#include <QPointF>
#include <QtMath>

// A QChartView that follows the mouse: it draws a vertical crosshair at the
// nearest age and reports net worth / income / expenses at that age into a
// QLabel. It also supports zooming: drag a rectangle to zoom into it, scroll
// the wheel to zoom in/out around the cursor, and double-click (or right-
// click) to reset back to the full view. No Qt signals are added, so this
// stays a header-only class (no moc).
class HoverChartView : public QChartView
{
public:
    explicit HoverChartView(QChart *chart, QWidget *parent = nullptr)
        : QChartView(chart, parent)
    {
        setMouseTracking(true);
        if (viewport())
            viewport()->setMouseTracking(true);
        setRubberBand(QChartView::RectangleRubberBand);
    }

    void setDataSeries(QLineSeries *nw, QLineSeries *inc, QLineSeries *exp)
    {
        nwS = nw; incS = inc; expS = exp;
    }
    void setCrosshair(QLineSeries *c) { crossS = c; }
    void setValueAxis(QValueAxis *y)  { axisY = y; }
    void setReadout(QLabel *l)        { readout = l; if (readout) readout->setText(hint()); }

protected:
    void mouseMoveEvent(QMouseEvent *event) override
    {
        // While a zoom rubber-band drag is in progress, leave the crosshair
        // alone so it doesn't fight the selection rectangle for attention.
        if (!(event->buttons() & Qt::LeftButton))
            updateForPos(event);
        QChartView::mouseMoveEvent(event);
    }
    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        chart()->zoomReset();
        QChartView::mouseDoubleClickEvent(event);
    }
    void wheelEvent(QWheelEvent *event) override
    {
        if (event->angleDelta().y() > 0)
            chart()->zoom(1.15);
        else if (event->angleDelta().y() < 0)
            chart()->zoom(1.0 / 1.15);
        event->accept();
    }
    void leaveEvent(QEvent *event) override
    {
        clearHover();
        QChartView::leaveEvent(event);
    }

private:
    static QString hint()
    {
        return QStringLiteral("Hover over the chart to read values at each age.");
    }

    QString money(double v) const
    {
        QLocale loc;
        return (v < 0 ? QStringLiteral("-$") : QStringLiteral("$"))
               + loc.toString(qAbs(v), 'f', 0);
    }

    void clearHover()
    {
        if (crossS)  crossS->clear();
        if (readout) readout->setText(hint());
    }

    void updateForPos(QMouseEvent *event)
    {
        if (!nwS || !incS || !expS || nwS->count() == 0)
            return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QPoint p = event->position().toPoint();
#else
        const QPoint p = event->pos();
#endif
        const QPointF val =
            chart()->mapToValue(chart()->mapFromScene(mapToScene(p)), nwS);
        const double ageF = val.x();

        const int minAge = qRound(nwS->at(0).x());
        const int maxAge = qRound(nwS->at(nwS->count() - 1).x());

        if (ageF < minAge - 0.5 || ageF > maxAge + 0.5) {
            clearHover();
            return;
        }

        int age = qRound(ageF);
        if (age < minAge) age = minAge;
        if (age > maxAge) age = maxAge;

        const int idx = age - minAge;
        if (idx < 0 || idx >= nwS->count()
            || idx >= incS->count() || idx >= expS->count()) {
            clearHover();
            return;
        }

        const double nwv  = nwS->at(idx).y();
        const double incv = incS->at(idx).y();
        const double expv = expS->at(idx).y();

        if (crossS && axisY) {
            crossS->clear();
            crossS->append(age, axisY->min());
            crossS->append(age, axisY->max());
        }

        if (readout) {
            readout->setText(QString(
                "<b>Age %1</b> &nbsp;&nbsp; "
                "Net worth: <b style='color:#1f77b4;'>%2</b> &nbsp;&nbsp; "
                "Income: <b style='color:#2ca02c;'>%3</b> &nbsp;&nbsp; "
                "Expenses: <b style='color:#d62728;'>%4</b>")
                .arg(age)
                .arg(money(nwv), money(incv), money(expv)));
        }
    }

    QLineSeries *nwS = nullptr, *incS = nullptr, *expS = nullptr;
    QLineSeries *crossS  = nullptr;
    QValueAxis  *axisY   = nullptr;
    QLabel      *readout = nullptr;
};

#endif // HOVERCHARTVIEW_H
