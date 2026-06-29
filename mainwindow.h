#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QString>

class QJsonObject;

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include "hoverchartview.h"

// Qt5's chart classes live in the QtCharts namespace; Qt6 removed it.
// QT_CHARTS_USE_NAMESPACE expands to `using namespace QtCharts;` on Qt5 and to
// nothing on Qt6. If the macro is missing entirely, define it as a no-op.
#ifndef QT_CHARTS_USE_NAMESPACE
#define QT_CHARTS_USE_NAMESPACE
#endif
QT_CHARTS_USE_NAMESPACE

class QSpinBox;
class QDoubleSpinBox;
class QTableWidget;
class QLabel;
class QPushButton;

struct Investment {
    QString name;
    double  value;   // current balance
    double  growth;  // projected annual growth, percent (e.g. 7.0)
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void addInvestmentRow();
    void removeInvestmentRow();
    void recalculate();
    void openFile();
    void saveFile();
    void saveFileAs();

private:
    QWidget *buildInputPanel();
    void     setupChart();
    void     createMenu();
    void     insertInvestment(const QString &name, double value, double growth);
    QVector<Investment> collectInvestments() const;

    QJsonObject toJson() const;
    void        applyJson(const QJsonObject &obj);
    bool        writeToFile(const QString &path);
    bool        readFromFile(const QString &path);
    void        updateTitle();

    QString currentFilePath;

    // --- input widgets ---
    QSpinBox       *currentAgeSpin   = nullptr;
    QSpinBox       *retireAgeSpin    = nullptr;
    QSpinBox       *endAgeSpin       = nullptr;
    QDoubleSpinBox *netWorthSpin     = nullptr;
    QDoubleSpinBox *debtSpin         = nullptr;
    QDoubleSpinBox *debtRateSpin     = nullptr;
    QDoubleSpinBox *debtPaymentSpin  = nullptr;
    QDoubleSpinBox *monthlyCostSpin  = nullptr;
    QDoubleSpinBox *inflationSpin    = nullptr;
    QDoubleSpinBox *incomeSpin       = nullptr;
    QDoubleSpinBox *raiseSpin        = nullptr;
    QDoubleSpinBox *pensionSpin      = nullptr;
    QSpinBox       *pensionAgeSpin   = nullptr;

    QTableWidget   *investmentTable  = nullptr;
    QPushButton    *addBtn           = nullptr;
    QPushButton    *removeBtn        = nullptr;
    QPushButton    *calcBtn          = nullptr;
    QLabel         *summaryLabel     = nullptr;

    // --- chart ---
    QChart         *chart          = nullptr;
    HoverChartView *chartView      = nullptr;
    QLineSeries  *netWorthSeries   = nullptr;
    QLineSeries  *incomeSeries     = nullptr;
    QLineSeries  *expenseSeries    = nullptr;
    QLineSeries  *debtPaidMarker   = nullptr;
    QLineSeries  *hoverSeries      = nullptr;
    QValueAxis   *axisX            = nullptr;
    QValueAxis   *axisY            = nullptr;
    QLabel       *hoverLabel       = nullptr;
};

#endif // MAINWINDOW_H
