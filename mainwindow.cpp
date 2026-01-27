#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QRegularExpression>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QHeaderView>
#include "rhythmic_delivery.h"
#include "pcplp.h"
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QChart>
#include <QDialog>
#include <QVBoxLayout>
#include <QtCharts/QHorizontalStackedBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QLegendMarker>
#include <QGraphicsLineItem>
#include <QGraphicsPolygonItem>
#include <cmath>

// start/finish: 0..N-1
// preds[j] содержит индексы предшественников работы j
static void showGanttChartWindow(QWidget* parent,
                                 const std::vector<int>& start,
                                 const std::vector<int>& finish,
                                 const std::vector<std::vector<int>>& preds,
                                 int cmax)
{
    const int N = (int)start.size();
    if (N == 0) return;

    // --- окно ---
    auto* dlg = new QDialog(parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(QString("Диаграмма Ганта (Cmax=%1)").arg(cmax));
    dlg->resize(1100, 650);

    auto* layout = new QVBoxLayout(dlg);

    // --- серия "смещение + длительность" ---
    auto* offsetSet = new QBarSet("start");
    auto* durSet    = new QBarSet("work");

    int maxFinish = 0;
    for (int j = 0; j < N; ++j) {
        const int s = start[j];
        const int f = finish[j];
        const int d = std::max(0, f - s);

        *offsetSet << s;
        *durSet    << d;
        maxFinish = std::max(maxFinish, f);
    }

    // "offset" делаем прозрачным, чтобы бар начинался с start[j]
    offsetSet->setBrush(Qt::transparent);
    offsetSet->setPen(Qt::NoPen);

    auto* series = new QHorizontalStackedBarSeries();
    series->append(offsetSet);
    series->append(durSet);

    auto* chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("ЗКПР — диаграмма Ганта");
    chart->legend()->setAlignment(Qt::AlignBottom);

    // скрыть легенду для offset
    auto markers = chart->legend()->markers(series);
    if (markers.size() >= 1) markers[0]->setVisible(false); // offset

    // --- ось Y (работы) ---
    auto* axisY = new QBarCategoryAxis();
    QStringList cats;
    for (int j = 0; j < N; ++j) cats << QString("Job %1").arg(j);
    axisY->append(cats);
    axisY->setReverse(true); // как в matplotlib (верх — Job 0)
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    // --- ось X (время) ---
    auto* axisX = new QValueAxis();
    axisX->setTitleText("Time");
    axisX->setRange(0, std::max(maxFinish, cmax) + 1);
    axisX->setTickCount(std::min(std::max(6, axisX->max() > 20 ? 10 : 6), 15));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto* view = new QChartView(chart);
    view->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(view);

    // -------- стрелки предшествования --------
    auto updateArrows = [chart, series, N, start, finish, preds]() {
        // удалить старые стрелки
        for (auto* item : chart->scene()->items()) {
            if (item->data(0).toString() == "predArrow") {
                chart->scene()->removeItem(item);
                delete item;
            }
        }

        auto addArrow = [&](QPointF p0, QPointF p1) {
            // линия
            auto* line = new QGraphicsLineItem(QLineF(p0, p1));
            line->setData(0, "predArrow");
            line->setPen(QPen(Qt::black, 1.0));
            chart->scene()->addItem(line);

            // наконечник
            const double ang = std::atan2(p1.y() - p0.y(), p1.x() - p0.x());
            const double L = 10.0;
            QPointF a = p1 - QPointF(L * std::cos(ang - 0.35), L * std::sin(ang - 0.35));
            QPointF b = p1 - QPointF(L * std::cos(ang + 0.35), L * std::sin(ang + 0.35));

            QPolygonF head;
            head << p1 << a << b;

            auto* poly = new QGraphicsPolygonItem(head);
            poly->setData(0, "predArrow");
            poly->setBrush(Qt::black);
            poly->setPen(Qt::NoPen);
            chart->scene()->addItem(poly);
        };

        // y в bar-chart — индекс категории (0..N-1)
        // рисуем от finish[pred] к start[job]
        for (int j = 0; j < N; ++j) {
            for (int pr : preds[j]) {
                if (pr < 0 || pr >= N) continue;

                double x0 = finish[pr];
                double y0 = pr;
                double x1 = start[j];
                double y1 = j;

                // небольшой сдвиг, чтобы стрелка не "втыкалась" в бар
                const double eps = 0.05;
                x0 = x0 - eps;
                x1 = x1 + eps;

                QPointF p0 = chart->mapToPosition(QPointF(x0, y0), series);
                QPointF p1 = chart->mapToPosition(QPointF(x1, y1), series);
                addArrow(p0, p1);
            }
        }
    };

    updateArrows();
    QObject::connect(chart, &QChart::plotAreaChanged, dlg, [=](const QRectF&) {
        updateArrows();
    });

    dlg->show();
}



static QVector<double> toQVec(const std::vector<double>& v)
{
    return QVector<double>(v.begin(), v.end());
}



static Vecr parseVecr(const QString& text) {
    Vecr v;
    const auto parts = text.split(QRegularExpression("[,;\\s]+"),
                                  Qt::SkipEmptyParts);
    v.reserve(parts.size());
    for (const auto& s : parts) v.push_back(s.toDouble());
    return v;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}



static void setNiceYRange(QChart* chart,
                          const QVector<double>& p,
                          const QVector<double>& x,
                          const QVector<double>& V,
                          double minV,
                          double maxV)
{
    double mn = minV;
    double mx = maxV;

    auto upd = [&](const QVector<double>& a){
        for (double v : a) { mn = std::min(mn, v); mx = std::max(mx, v); }
    };
    upd(p); upd(x); upd(V);

    // небольшой запас по краям
    double pad = (mx - mn) * 0.08;
    if (pad <= 0) pad = 1.0;
    mn -= pad;
    mx += pad;

    auto axY = qobject_cast<QValueAxis*>(chart->axes(Qt::Vertical).value(0, nullptr));
    if (!axY) return;

    axY->setRange(mn, mx);
}

QChartView* makeRhythmicChart(const QVector<double>& p,
                                        const QVector<double>& x,
                                        const QVector<double>& V,
                                        double minV,
                                        double maxV)
{

    const int T = p.size();

    auto sP   = new QLineSeries();  sP->setName("p");
    auto sX   = new QLineSeries();  sX->setName("x");
    auto sV   = new QLineSeries();  sV->setName("V");
    auto sMin = new QLineSeries();  sMin->setName("minV");
    auto sMax = new QLineSeries();  sMax->setName("maxV");

    for (int i = 0; i < T; ++i) {
        double t = i + 1;
        sP->append(t, p[i]);
        sX->append(t, x[i]);
        sV->append(t, V[i]);
        sMin->append(t, minV);
        sMax->append(t, maxV);
    }

    auto chart = new QChart();
    chart->addSeries(sP);
    chart->addSeries(sX);
    chart->addSeries(sV);
    chart->addSeries(sMin);
    chart->addSeries(sMax);

    chart->setTitle("Ритмичные поставки");
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignTop);

    // Ось X (t = 1..T)
    auto axX = new QValueAxis();
    axX->setTitleText("t");
    axX->setRange(1, T);
    axX->setLabelFormat("%d");
    axX->setTickCount(std::min(T, 12) + 1); // чтобы не было слишком мелко

    // Ось Y
    auto axY = new QValueAxis();
    axY->setTitleText("value");
    axY->setLabelFormat("%.2f");

    chart->addAxis(axX, Qt::AlignBottom);
    chart->addAxis(axY, Qt::AlignLeft);

    for (auto s : {sP, sX, sV, sMin, sMax}) {
        s->attachAxis(axX);
        s->attachAxis(axY);
    }

    // ВАЖНО: выставляем диапазон Y так, чтобы minV/maxV точно попали
    setNiceYRange(chart, p, x, V, minV, maxV);

    auto view = new QChartView(chart);
    view->setRenderHint(QPainter::Antialiasing, true);
    return view;
}



void MainWindow::on_solveBtn_clicked()
{
    const Vecr p = parseVecr(ui->pEdit->toPlainText());
    if (p.empty()) {
        QMessageBox::warning(this, "Ошибка", "Введите массив p (числа через пробел/перенос строки).");
        return;
    }

    const double V0   = ui->v0Spin->value();
    const double minV = ui->minVSpin->value();
    const double maxV = ui->maxVSpin->value();

    if (minV > maxV) {
        QMessageBox::warning(this, "Ошибка", "minV не может быть больше maxV.");
        return;
    }

    // Вызов твоего PG-метода
    const UniformityIterResult res =
        solve_rhythmic_delivery_uniform_pg(p, V0, minV, maxV);

    // Настроим таблицу
    ui->resultTable->clear();
    ui->resultTable->setColumnCount(4);
    ui->resultTable->setRowCount(static_cast<int>(p.size()));
    ui->resultTable->setHorizontalHeaderLabels({"t", "p[t]", "x[t]", "V[t]"});

    // На всякий случай: если x/V не той длины, не падаем
    const int n = static_cast<int>(p.size());
    const int nx = static_cast<int>(res.x.size());
    const int nV = static_cast<int>(res.V.size());

    for (int t = 0; t < n; ++t) {
        ui->resultTable->setItem(t, 0, new QTableWidgetItem(QString::number(t + 1)));
        ui->resultTable->setItem(t, 1, new QTableWidgetItem(QString::number(p[t])));

        const QString xStr = (t < nx) ? QString::number(res.x[t]) : "-";
        const QString vStr = (t < nV) ? QString::number(res.V[t]) : "-";

        ui->resultTable->setItem(t, 2, new QTableWidgetItem(xStr));
        ui->resultTable->setItem(t, 3, new QTableWidgetItem(vStr));
    }

    ui->resultTable->resizeColumnsToContents();

    const QString status =
        QString("ok=%1 | Mp=%2 | iters=%3/%4")
            .arg(res.ok ? "true" : "false")
            .arg(res.Mp)
            .arg(res.iters)
            .arg(res.maxIter);

    // Если добавил label:
    if (ui->statusLabel) ui->statusLabel->setText(status);
    else QMessageBox::information(this, "Результат", status);
    auto* view = makeRhythmicChart(toQVec(p), toQVec(res.x), toQVec(res.V), minV, maxV);
    if (view) {
        auto* w = new QWidget();
        w->setWindowTitle("График");
        auto* lay = new QVBoxLayout(w);
        lay->addWidget(view);
        w->resize(1000, 700);
        w->show();
    }

}


void MainWindow::on_solveDirectBtn_clicked()
{
    const Vecr p = parseVecr(ui->pEdit->toPlainText());
    if (p.empty()) {
        QMessageBox::warning(this, "Ошибка", "Введите массив p (числа через пробел/перенос строки).");
        return;
    }

    const double V0   = ui->v0Spin->value();
    const double minV = ui->minVSpin->value();
    const double maxV = ui->maxVSpin->value();

    if (minV > maxV) {
        QMessageBox::warning(this, "Ошибка", "minV не может быть больше maxV.");
        return;
    }

    const DeliveryResult res =
        solve_rhythmic_delivery_bounds_direct(p, V0, minV, maxV);

    // Таблица такая же (t, p[t], x[t], V[t])
    ui->resultTable->clear();
    ui->resultTable->setColumnCount(4);
    ui->resultTable->setRowCount(static_cast<int>(p.size()));
    ui->resultTable->setHorizontalHeaderLabels({"t", "p[t]", "x[t]", "V[t]"});

    const int n  = static_cast<int>(p.size());
    const int nx = static_cast<int>(res.x.size());
    const int nV = static_cast<int>(res.V.size());

    for (int t = 0; t < n; ++t) {
        ui->resultTable->setItem(t, 0, new QTableWidgetItem(QString::number(t + 1)));
        ui->resultTable->setItem(t, 1, new QTableWidgetItem(QString::number(p[t])));

        const QString xStr = (t < nx) ? QString::number(res.x[t]) : "-";
        const QString vStr = (t < nV) ? QString::number(res.V[t]) : "-";

        ui->resultTable->setItem(t, 2, new QTableWidgetItem(xStr));
        ui->resultTable->setItem(t, 3, new QTableWidgetItem(vStr));
    }

    ui->resultTable->resizeColumnsToContents();

    const QString status = QString("ok=%1 | direct method")
                               .arg(res.ok ? "true" : "false");

    // Если ты используешь статус как label:
    ui->statusLabel->setText(status);
    // Если statusLabel нет — замени на QMessageBox::information(...)
    auto* view = makeRhythmicChart(toQVec(p), toQVec(res.x), toQVec(res.V), minV, maxV);
    if (view) {
        auto* w = new QWidget();
        w->setWindowTitle("График");
        auto* lay = new QVBoxLayout(w);
        lay->addWidget(view);
        w->resize(1000, 700);
        w->show();
    }

}


static int cellInt(QTableWidget* t, int r, int c, int def = 0) {
    if (!t) return def;
    auto* it = t->item(r, c);
    if (!it) return def;
    bool ok = false;
    int v = it->text().trimmed().toInt(&ok);
    return ok ? v : def;
}

static VecVecPairii readDemandsPairs(QTableWidget* demandsTable, int N, int M) {
    VecVecPairii d;
    d.resize(N);
    for (int i = 0; i < N; ++i) {
        for (int m = 0; m < M; ++m) {
            int a = cellInt(demandsTable, i, m, 0);
            if (a != 0) d[i].push_back({m, a});
        }
    }
    return d;
}

static VecVeci readPreds(QTableWidget* predsTable, int N) {
    VecVeci preds;
    preds.resize(N);
    for (int i = 0; i < N; ++i) {
        auto* it = predsTable->item(i, 0);
        if (!it) continue;
        const QString s = it->text();
        const auto parts = s.split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
        for (const auto& p : parts) preds[i].push_back(p.toInt());
    }
    return preds;
}

static void niceTable(QTableWidget* t) {
    if (!t) return;
    t->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    t->setSelectionBehavior(QAbstractItemView::SelectItems);
    t->setAlternatingRowColors(true);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}




void MainWindow::on_buildZkprBtn_clicked()
{
    const int N = ui->nSpin->value();
    const int M = ui->mSpin->value();

    if (N <= 0 || M <= 0) {
        QMessageBox::warning(this, "Ошибка", "N и M должны быть > 0");
        return;
    }

    // jobsTable: N x 2
    ui->jobsTable->setRowCount(N);
    ui->jobsTable->setColumnCount(2);
    ui->jobsTable->setHorizontalHeaderLabels({"dur", "rel"});

    // capTable: 1 x M
    ui->capTable->setRowCount(1);
    ui->capTable->setColumnCount(M);
    QStringList capHeaders;
    for (int m = 0; m < M; ++m) capHeaders << QString("R%1").arg(m);
    ui->capTable->setHorizontalHeaderLabels(capHeaders);

    // demandsTable: N x M
    ui->demandsTable->setRowCount(N);
    ui->demandsTable->setColumnCount(M);
    ui->demandsTable->setHorizontalHeaderLabels(capHeaders);

    // predsTable: N x 1
    ui->predsTable->setRowCount(N);
    ui->predsTable->setColumnCount(1);
    ui->predsTable->setHorizontalHeaderLabels({"preds (через пробел)"});

    // scheduleTable: N x 3 (пока пустая)
    ui->scheduleTable->setRowCount(N);
    ui->scheduleTable->setColumnCount(3);
    ui->scheduleTable->setHorizontalHeaderLabels({"job", "start", "finish"});

    niceTable(ui->jobsTable);
    niceTable(ui->capTable);
    niceTable(ui->demandsTable);
    niceTable(ui->predsTable);
    niceTable(ui->scheduleTable);

    // ui->zkprStatusLabel->setText("Таблицы созданы/обновлены.");
}



void MainWindow::on_solveZkprBtn_clicked()
{
    const int N = ui->nSpin->value();
    const int M = ui->mSpin->value();

    if (N <= 0 || M <= 0) {
        QMessageBox::warning(this, "Ошибка", "N и M должны быть > 0");
        return;
    }

    // dur/rel
    Veci dur; dur.reserve(N);
    Veci rel; rel.reserve(N);
    for (int i = 0; i < N; ++i) {
        dur.push_back(cellInt(ui->jobsTable, i, 0, 0));
        rel.push_back(cellInt(ui->jobsTable, i, 1, 0));
    }

    // cap
    Veci cap; cap.reserve(M);
    for (int m = 0; m < M; ++m) cap.push_back(cellInt(ui->capTable, 0, m, 0));

    // demands (только ненулевые)
    const VecVecPairii demands = readDemandsPairs(ui->demandsTable, N, M);

    // preds (строкой)
    const VecVeci preds = readPreds(ui->predsTable, N);

    // Решаем
    const Schedule s = solve_PCPLP(N, M, dur, rel, cap, demands, preds);

    // Вывод
    ui->scheduleTable->setRowCount(N);
    ui->scheduleTable->setColumnCount(3);
    ui->scheduleTable->setHorizontalHeaderLabels({"job", "start", "finish"});

    for (int i = 0; i < N; ++i) {
        ui->scheduleTable->setItem(i, 0, new QTableWidgetItem(QString::number(i)));
        ui->scheduleTable->setItem(i, 1, new QTableWidgetItem(i < (int)s.start.size() ? QString::number(s.start[i]) : "-"));
        ui->scheduleTable->setItem(i, 2, new QTableWidgetItem(i < (int)s.finish.size() ? QString::number(s.finish[i]) : "-"));
    }

    // ui->zkprStatusLabel->setText(QString("Готово. cmax=%1").arg(s.cmax));
    showGanttChartWindow(this, s.start, s.finish, preds, s.cmax);
}



