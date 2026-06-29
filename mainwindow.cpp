#include "mainwindow.h"

#include <QWidget>
#include <QGroupBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QKeySequence>
#include <QLocale>
#include <QStringList>
#include <QPainter>
#include <QPen>
#include <QColor>
#include <QtCharts/QLegend>
#include <QtCharts/QLegendMarker>

#include <cmath>
#include <limits>
#include <algorithm>

// ---- small helpers to build configured spin boxes -------------------------

static QDoubleSpinBox *makeMoneySpin(double value)
{
    auto *s = new QDoubleSpinBox;
    s->setRange(0.0, 1.0e9);
    s->setDecimals(0);
    s->setSingleStep(1000.0);
    s->setPrefix("$ ");
    s->setGroupSeparatorShown(true);
    s->setValue(value);
    return s;
}

static QDoubleSpinBox *makeRateSpin(double value)
{
    auto *s = new QDoubleSpinBox;
    s->setRange(-50.0, 100.0);
    s->setDecimals(1);
    s->setSingleStep(0.5);
    s->setSuffix(" %");
    s->setValue(value);
    return s;
}

static QSpinBox *makeAgeSpin(int value)
{
    auto *s = new QSpinBox;
    s->setRange(0, 120);
    s->setValue(value);
    return s;
}

// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    createMenu();

    auto *central = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(central);

    // ---- left: scrollable input panel ----
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(buildInputPanel());
    scroll->setMinimumWidth(380);
    scroll->setMaximumWidth(440);
    rootLayout->addWidget(scroll);

    // ---- right: chart + notes + summary ----
    setupChart();

    hoverLabel = new QLabel;
    hoverLabel->setTextFormat(Qt::RichText);
    hoverLabel->setStyleSheet(
        "QLabel { background:#f4f4f4; border:1px solid #ddd; border-radius:4px;"
        " padding:5px 8px; font-size:12px; }");
    hoverLabel->setMinimumHeight(28);
    chartView->setReadout(hoverLabel);

    auto *noteLabel = new QLabel(
        "Income = salary + pension/Social Security + investment gains.   "
        "Expenses = living costs + debt payments.\n"
        "All figures are nominal (future dollars, not adjusted back to today).");
    noteLabel->setWordWrap(true);
    noteLabel->setStyleSheet("color: #555; font-size: 11px;");

    summaryLabel = new QLabel;
    summaryLabel->setWordWrap(true);
    summaryLabel->setTextFormat(Qt::RichText);
    summaryLabel->setStyleSheet("font-size: 12px;");

    auto *rightLayout = new QVBoxLayout;
    rightLayout->addWidget(chartView, /*stretch*/ 1);
    rightLayout->addWidget(hoverLabel);
    rightLayout->addWidget(noteLabel);
    rightLayout->addWidget(summaryLabel);
    rootLayout->addLayout(rightLayout, /*stretch*/ 1);

    setCentralWidget(central);

    // ---- wire up live recalculation ----
    const QList<QDoubleSpinBox *> dspins = {
        netWorthSpin, debtSpin, debtRateSpin, debtPaymentSpin,
        monthlyCostSpin, inflationSpin, incomeSpin, raiseSpin, pensionSpin };
    for (auto *s : dspins)
        connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &MainWindow::recalculate);

    const QList<QSpinBox *> ispins = {
        currentAgeSpin, retireAgeSpin, endAgeSpin, pensionAgeSpin };
    for (auto *s : ispins)
        connect(s, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &MainWindow::recalculate);

    connect(investmentTable, &QTableWidget::cellChanged,
            this, &MainWindow::recalculate);
    connect(addBtn,    &QPushButton::clicked, this, &MainWindow::addInvestmentRow);
    connect(removeBtn, &QPushButton::clicked, this, &MainWindow::removeInvestmentRow);
    connect(calcBtn,   &QPushButton::clicked, this, &MainWindow::recalculate);

    updateTitle();
    recalculate();
}

// ---------------------------------------------------------------------------

QWidget *MainWindow::buildInputPanel()
{
    auto *panel = new QWidget;
    auto *outer = new QVBoxLayout(panel);

    // --- About you ---
    auto *youBox = new QGroupBox("About you");
    auto *youForm = new QFormLayout(youBox);
    currentAgeSpin = makeAgeSpin(30);
    retireAgeSpin  = makeAgeSpin(65);
    endAgeSpin     = makeAgeSpin(100);
    youForm->addRow("Current age",          currentAgeSpin);
    youForm->addRow("Planned retirement age", retireAgeSpin);
    youForm->addRow("Project until age",    endAgeSpin);
    outer->addWidget(youBox);

    // --- Money now ---
    auto *nowBox = new QGroupBox("Finances today");
    auto *nowForm = new QFormLayout(nowBox);
    netWorthSpin    = makeMoneySpin(50000);
    debtSpin        = makeMoneySpin(15000);
    debtRateSpin    = makeRateSpin(5.0);
    debtPaymentSpin = makeMoneySpin(6000);
    nowForm->addRow("Current net worth",        netWorthSpin);
    nowForm->addRow("Current debts",            debtSpin);
    nowForm->addRow("Debt interest rate",       debtRateSpin);
    nowForm->addRow("Annual debt payment",      debtPaymentSpin);
    outer->addWidget(nowBox);

    // --- Income & expenses ---
    auto *flowBox = new QGroupBox("Income & expenses");
    auto *flowForm = new QFormLayout(flowBox);
    monthlyCostSpin = makeMoneySpin(4000);
    inflationSpin   = makeRateSpin(3.0);
    incomeSpin      = makeMoneySpin(80000);
    raiseSpin       = makeRateSpin(3.0);
    pensionSpin     = makeMoneySpin(24000);
    pensionAgeSpin  = makeAgeSpin(67);
    flowForm->addRow("Estimated monthly costs",        monthlyCostSpin);
    flowForm->addRow("Estimated inflation",            inflationSpin);
    flowForm->addRow("Annual income (while working)",  incomeSpin);
    flowForm->addRow("Annual raise / income growth",   raiseSpin);
    flowForm->addRow("Annual pension / Soc. Security", pensionSpin);
    flowForm->addRow("Pension / SS start age",         pensionAgeSpin);
    outer->addWidget(flowBox);

    // --- Investments ---
    auto *invBox = new QGroupBox("Investments");
    auto *invLayout = new QVBoxLayout(invBox);
    investmentTable = new QTableWidget(0, 3);
    investmentTable->setHorizontalHeaderLabels(
        QStringList() << "Name" << "Current value ($)" << "Growth (%/yr)");
    investmentTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    investmentTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    investmentTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    investmentTable->verticalHeader()->setVisible(false);
    investmentTable->setMinimumHeight(160);
    invLayout->addWidget(investmentTable);

    auto *btnRow = new QHBoxLayout;
    addBtn    = new QPushButton("Add investment");
    removeBtn = new QPushButton("Remove selected");
    btnRow->addWidget(addBtn);
    btnRow->addWidget(removeBtn);
    invLayout->addLayout(btnRow);
    outer->addWidget(invBox);

    // a couple of sensible starting investments
    insertInvestment("Index fund",   50000, 7.0);
    insertInvestment("Bonds",        10000, 3.5);

    calcBtn = new QPushButton("Recalculate");
    outer->addWidget(calcBtn);
    outer->addStretch(1);

    return panel;
}

void MainWindow::setupChart()
{
    chart = new QChart;
    chart->setTitle("Net worth, income and expenses over time");

    netWorthSeries = new QLineSeries; netWorthSeries->setName("Net worth");
    incomeSeries   = new QLineSeries; incomeSeries->setName("Yearly income");
    expenseSeries  = new QLineSeries; expenseSeries->setName("Yearly expenses");
    debtPaidMarker = new QLineSeries; debtPaidMarker->setName("Debt paid off");
    hoverSeries    = new QLineSeries; hoverSeries->setName("Cursor");

    QPen p1(QColor("#1f77b4")); p1.setWidth(2); netWorthSeries->setPen(p1);
    QPen p2(QColor("#2ca02c")); p2.setWidth(2); incomeSeries->setPen(p2);
    QPen p3(QColor("#d62728")); p3.setWidth(2); expenseSeries->setPen(p3);
    QPen pm(QColor("#ff7f0e")); pm.setWidth(2); pm.setStyle(Qt::DashLine);
    debtPaidMarker->setPen(pm);
    QPen ph(QColor("#888888")); ph.setWidth(1); ph.setStyle(Qt::DotLine);
    hoverSeries->setPen(ph);

    chart->addSeries(netWorthSeries);
    chart->addSeries(incomeSeries);
    chart->addSeries(expenseSeries);
    chart->addSeries(debtPaidMarker);
    chart->addSeries(hoverSeries);

    axisX = new QValueAxis;
    axisX->setTitleText("Age");
    axisX->setLabelFormat("%.0f");

    axisY = new QValueAxis;
    axisY->setTitleText("US dollars");
    axisY->setLabelFormat("$%.0f");

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    for (auto *s : { netWorthSeries, incomeSeries, expenseSeries, debtPaidMarker, hoverSeries }) {
        s->attachAxis(axisX);
        s->attachAxis(axisY);
    }

    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    // keep the crosshair helper line out of the legend
    const auto hoverMarkers = chart->legend()->markers(hoverSeries);
    for (QLegendMarker *m : hoverMarkers)
        m->setVisible(false);

    chartView = new HoverChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setDataSeries(netWorthSeries, incomeSeries, expenseSeries);
    chartView->setCrosshair(hoverSeries);
    chartView->setValueAxis(axisY);
}

// ---------------------------------------------------------------------------
//  Menu + save / load
// ---------------------------------------------------------------------------

void MainWindow::createMenu()
{
    QMenu *fileMenu = menuBar()->addMenu("&File");

    QAction *openAct = fileMenu->addAction("&Open...");
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::openFile);

    QAction *saveAct = fileMenu->addAction("&Save");
    saveAct->setShortcut(QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveFile);

    QAction *saveAsAct = fileMenu->addAction("Save &As...");
    saveAsAct->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::saveFileAs);

    fileMenu->addSeparator();
    QAction *quitAct = fileMenu->addAction("E&xit");
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::updateTitle()
{
    const QString name = currentFilePath.isEmpty()
            ? QStringLiteral("Untitled")
            : QFileInfo(currentFilePath).fileName();
    setWindowTitle(QString("Retirement Planner \u2014 %1").arg(name));
}

QJsonObject MainWindow::toJson() const
{
    QJsonObject o;
    o["version"]      = 1;
    o["currentAge"]   = currentAgeSpin->value();
    o["retireAge"]    = retireAgeSpin->value();
    o["endAge"]       = endAgeSpin->value();
    o["netWorth"]     = netWorthSpin->value();
    o["debt"]         = debtSpin->value();
    o["debtRate"]     = debtRateSpin->value();
    o["debtPayment"]  = debtPaymentSpin->value();
    o["monthlyCost"]  = monthlyCostSpin->value();
    o["inflation"]    = inflationSpin->value();
    o["income"]       = incomeSpin->value();
    o["raise"]        = raiseSpin->value();
    o["pension"]      = pensionSpin->value();
    o["pensionAge"]   = pensionAgeSpin->value();

    QJsonArray arr;
    for (const Investment &inv : collectInvestments()) {
        QJsonObject io;
        io["name"]   = inv.name;
        io["value"]  = inv.value;
        io["growth"] = inv.growth;
        arr.append(io);
    }
    o["investments"] = arr;
    return o;
}

void MainWindow::applyJson(const QJsonObject &o)
{
    auto setI = [](QSpinBox *s, int v)       { s->blockSignals(true); s->setValue(v); s->blockSignals(false); };
    auto setD = [](QDoubleSpinBox *s, double v){ s->blockSignals(true); s->setValue(v); s->blockSignals(false); };

    setI(currentAgeSpin,  o.value("currentAge").toInt(currentAgeSpin->value()));
    setI(retireAgeSpin,   o.value("retireAge").toInt(retireAgeSpin->value()));
    setI(endAgeSpin,      o.value("endAge").toInt(endAgeSpin->value()));
    setD(netWorthSpin,    o.value("netWorth").toDouble(netWorthSpin->value()));
    setD(debtSpin,        o.value("debt").toDouble(debtSpin->value()));
    setD(debtRateSpin,    o.value("debtRate").toDouble(debtRateSpin->value()));
    setD(debtPaymentSpin, o.value("debtPayment").toDouble(debtPaymentSpin->value()));
    setD(monthlyCostSpin, o.value("monthlyCost").toDouble(monthlyCostSpin->value()));
    setD(inflationSpin,   o.value("inflation").toDouble(inflationSpin->value()));
    setD(incomeSpin,      o.value("income").toDouble(incomeSpin->value()));
    setD(raiseSpin,       o.value("raise").toDouble(raiseSpin->value()));
    setD(pensionSpin,     o.value("pension").toDouble(pensionSpin->value()));
    setI(pensionAgeSpin,  o.value("pensionAge").toInt(pensionAgeSpin->value()));

    investmentTable->blockSignals(true);
    investmentTable->setRowCount(0);
    investmentTable->blockSignals(false);
    const QJsonArray arr = o.value("investments").toArray();
    for (const auto &v : arr) {
        const QJsonObject io = v.toObject();
        insertInvestment(io.value("name").toString(),
                         io.value("value").toDouble(),
                         io.value("growth").toDouble());
    }

    recalculate();
}

bool MainWindow::writeToFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Save failed",
                             QString("Could not write to:\n%1").arg(path));
        return false;
    }
    f.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    f.close();
    currentFilePath = path;
    updateTitle();
    return true;
}

bool MainWindow::readFromFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Open failed",
                             QString("Could not read:\n%1").arg(path));
        return false;
    }
    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, "Open failed",
                             "This file is not a valid retirement plan.");
        return false;
    }
    applyJson(doc.object());
    currentFilePath = path;
    updateTitle();
    return true;
}

void MainWindow::openFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Open plan", currentFilePath,
        "Retirement plans (*.json);;All files (*)");
    if (!path.isEmpty())
        readFromFile(path);
}

void MainWindow::saveFile()
{
    if (currentFilePath.isEmpty())
        saveFileAs();
    else
        writeToFile(currentFilePath);
}

void MainWindow::saveFileAs()
{
    QString path = QFileDialog::getSaveFileName(
        this, "Save plan",
        currentFilePath.isEmpty() ? QStringLiteral("retirement.json") : currentFilePath,
        "Retirement plans (*.json);;All files (*)");
    if (path.isEmpty())
        return;
    if (!path.endsWith(".json", Qt::CaseInsensitive))
        path += ".json";
    writeToFile(path);
}

// ---------------------------------------------------------------------------

void MainWindow::insertInvestment(const QString &name, double value, double growth)
{
    investmentTable->blockSignals(true);
    const int row = investmentTable->rowCount();
    investmentTable->insertRow(row);
    investmentTable->setItem(row, 0, new QTableWidgetItem(name));
    investmentTable->setItem(row, 1, new QTableWidgetItem(QString::number(value)));
    investmentTable->setItem(row, 2, new QTableWidgetItem(QString::number(growth)));
    investmentTable->blockSignals(false);
}

void MainWindow::addInvestmentRow()
{
    insertInvestment(QString("Investment %1").arg(investmentTable->rowCount() + 1),
                     10000, 6.0);
    recalculate();
}

void MainWindow::removeInvestmentRow()
{
    int row = investmentTable->currentRow();
    if (row < 0)
        row = investmentTable->rowCount() - 1;
    if (row >= 0)
        investmentTable->removeRow(row);
    recalculate();
}

QVector<Investment> MainWindow::collectInvestments() const
{
    QVector<Investment> list;
    for (int r = 0; r < investmentTable->rowCount(); ++r) {
        Investment inv;
        const QTableWidgetItem *n = investmentTable->item(r, 0);
        const QTableWidgetItem *v = investmentTable->item(r, 1);
        const QTableWidgetItem *g = investmentTable->item(r, 2);
        inv.name   = n ? n->text() : QString("Investment %1").arg(r + 1);
        inv.value  = v ? v->text().toDouble() : 0.0;
        inv.growth = g ? g->text().toDouble() : 0.0;
        list.append(inv);
    }
    return list;
}

// ---------------------------------------------------------------------------
//  The projection: simulate one year at a time from current age to end age.
//
//  Model / assumptions (kept simple and documented on screen):
//   * Living costs grow with inflation every year.
//   * While working (age < retirement age) salary grows by the "raise" rate.
//   * Pension / Social Security begins at its start age and grows with inflation.
//   * Each investment compounds at its own growth rate.
//   * A "cash / other assets" bucket holds whatever isn't in the listed
//     investments. It is sized so that year-0 net worth equals the figure you
//     entered:   cash = net worth + debt - sum(investments).
//   * Yearly surplus (income - expenses) is reinvested across the portfolio.
//     A shortfall is drawn down from cash first, then from investments.
//   * Debt accrues interest and is paid down by the annual debt payment.
// ---------------------------------------------------------------------------

void MainWindow::recalculate()
{
    const int curAge = currentAgeSpin->value();
    const int retAge = retireAgeSpin->value();
    const int endAge = endAgeSpin->value();

    netWorthSeries->clear();
    incomeSeries->clear();
    expenseSeries->clear();
    debtPaidMarker->clear();
    hoverSeries->clear();

    if (endAge <= curAge) {
        summaryLabel->setText(
            "<b>Projection range is empty.</b> Set \"Project until age\" higher "
            "than the current age.");
        return;
    }

    const double infl     = inflationSpin->value()  / 100.0;
    const double raise     = raiseSpin->value()      / 100.0;
    const double debtRate = debtRateSpin->value()   / 100.0;
    const double annualExpenseBase = monthlyCostSpin->value() * 12.0;
    const double salaryBase        = incomeSpin->value();
    const double pension           = pensionSpin->value();
    const int    pensionAge        = pensionAgeSpin->value();
    const double debtPayment       = debtPaymentSpin->value();

    QVector<Investment> invs = collectInvestments();
    QVector<double> balances;
    balances.reserve(invs.size());
    double sumInvest = 0.0;
    for (const Investment &inv : invs) {
        balances.append(inv.value);
        sumInvest += inv.value;
    }

    double debt = debtSpin->value();
    const double initialDebt = debt;
    double cash = netWorthSpin->value() + debt - sumInvest; // other assets bucket

    QLocale loc;
    double minY = 0.0;
    double maxY = 0.0;
    double nwAtRetire = std::numeric_limits<double>::quiet_NaN();
    double finalNW = 0.0;
    int    depletionAge = -1;
    int    payoffAge = -1;

    for (int age = curAge; age <= endAge; ++age) {
        const int yr = age - curAge;
        const double inflFactor = std::pow(1.0 + infl, yr);

        const double livingExpense = annualExpenseBase * inflFactor;
        const double salary = (age < retAge)
                ? salaryBase * std::pow(1.0 + raise, yr) : 0.0;
        const double pensionInc = (age >= pensionAge) ? pension * inflFactor : 0.0;

        // debt: accrue interest, then pay
        double debtPaid = 0.0;
        if (debt > 0.0) {
            debt *= (1.0 + debtRate);
            debtPaid = std::min(debt, debtPayment);
            debt -= debtPaid;
            if (debt <= 1e-6 && payoffAge < 0) {
                debt = 0.0;
                payoffAge = age;   // debt cleared this year
            }
        }

        // investments grow
        double gains = 0.0;
        for (int i = 0; i < balances.size(); ++i) {
            const double g = balances[i] * (invs[i].growth / 100.0);
            balances[i] += g;
            gains += g;
        }

        // cash flow for the year
        const double netCash = salary + pensionInc - livingExpense - debtPaid;
        if (netCash >= 0.0) {
            double totalBal = 0.0;
            for (double b : balances) totalBal += b;
            if (totalBal > 0.0) {
                for (int i = 0; i < balances.size(); ++i)
                    balances[i] += netCash * (balances[i] / totalBal);
            } else {
                cash += netCash;
            }
        } else {
            double need = -netCash;
            if (cash > 0.0) {
                const double fromCash = std::min(cash, need);
                cash  -= fromCash;
                need  -= fromCash;
            }
            if (need > 0.0) {
                double totalBal = 0.0;
                for (double b : balances) totalBal += b;
                if (totalBal > 0.0) {
                    const double take = std::min(totalBal, need);
                    for (int i = 0; i < balances.size(); ++i)
                        balances[i] -= take * (balances[i] / totalBal);
                    need -= take;
                }
            }
            if (need > 0.0)
                cash -= need;   // ran out of assets -> shows up as negative net worth
        }

        double totalInvest = 0.0;
        for (double b : balances) totalInvest += b;

        const double netWorth = cash + totalInvest - debt;
        const double income   = salary + pensionInc + gains;
        const double expense  = livingExpense + debtPaid;

        netWorthSeries->append(age, netWorth);
        incomeSeries->append(age, income);
        expenseSeries->append(age, expense);

        minY = std::min({ minY, netWorth, income, expense });
        maxY = std::max({ maxY, netWorth, income, expense });

        if (age == retAge) nwAtRetire = netWorth;
        if (depletionAge < 0 && netWorth < 0.0) depletionAge = age;
        finalNW = netWorth;
    }

    axisX->setRange(curAge, endAge);
    const double pad = (maxY - minY) * 0.05;
    const double yLo = minY - pad;
    const double yHi = maxY + pad;
    axisY->setRange(yLo, yHi);

    // vertical marker line at the age debt is fully paid off
    const bool showMarker = (payoffAge >= 0);
    if (showMarker) {
        debtPaidMarker->append(payoffAge, yLo);
        debtPaidMarker->append(payoffAge, yHi);
        debtPaidMarker->setName(QString("Debt paid off (age %1)").arg(payoffAge));
    }
    debtPaidMarker->setVisible(showMarker);
    const auto markers = chart->legend()->markers(debtPaidMarker);
    for (QLegendMarker *m : markers)
        m->setVisible(showMarker);

    // summary text
    QString s = "<b>Summary</b><br/>";
    if (!std::isnan(nwAtRetire))
        s += QString("Net worth at retirement (age %1): <b>$%2</b><br/>")
                 .arg(retAge).arg(loc.toString(nwAtRetire, 'f', 0));
    else
        s += QString("Retirement age %1 is outside the projection range.<br/>")
                 .arg(retAge);

    s += QString("Net worth at age %1: <b>$%2</b><br/>")
             .arg(endAge).arg(loc.toString(finalNW, 'f', 0));

    if (initialDebt > 0.0) {
        if (payoffAge >= 0)
            s += QString("<span style='color:#ff7f0e;'>Debt paid off at age %1.</span><br/>")
                     .arg(payoffAge);
        else
            s += QString("<span style='color:#ff7f0e;'>Debt not fully paid off by age %1 "
                         "&mdash; raise the annual payment.</span><br/>")
                     .arg(endAge);
    }

    if (depletionAge >= 0)
        s += QString("<span style='color:#d62728;'>Funds run out at age %1.</span>")
                 .arg(depletionAge);
    else
        s += "<span style='color:#2ca02c;'>Net worth stays positive through the "
             "whole projection.</span>";

    summaryLabel->setText(s);
}
