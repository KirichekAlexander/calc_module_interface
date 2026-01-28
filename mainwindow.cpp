#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QRegularExpression>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QHeaderView>
#include "core/rhythmic_delivery.h"
#include "core/pcplp.h"
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


using namespace QtCharts;

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
    dlg->setWindowTitle(QString("Gantt chart (Cmax=%1)").arg(cmax));
    dlg->resize(1100, 650);

    auto* layout = new QVBoxLayout(dlg);

    // --- серия "смещение + длительность" ---
    auto* offsetSet = new QBarSet("start");
    auto* durSet    = new QBarSet("work");

    int maxFinish = 0;
    for (int j = N - 1; j >= 0; --j) {   // <-- ВАЖНО: N..1
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
    chart->setTitle("RCPSP - Gantt chart");
    chart->legend()->setAlignment(Qt::AlignBottom);

    // скрыть легенду для offset
    auto markers = chart->legend()->markers(series);
    if (markers.size() >= 1) markers[0]->setVisible(false); // offset

    // --- ось Y (работы) ---
    auto* axisY = new QBarCategoryAxis();
    QStringList cats;
    cats.reserve(N);

    // ВАЖНО: Job 1 .. Job N в естественном порядке
    for (int j = 0; j < N; ++j)
        cats << QString("Job %1").arg(N - j);

    axisY->append(cats);

    // ВАЖНО: reverse=true => первая категория (Job 1) окажется СВЕРХУ
    axisY->setReverse(false);

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
                double x1 = start[j];
                double y0 = (N - 1 - pr);
                double y1 = (N - 1 - j);

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
    return QVector<double>::fromStdVector(v);
}



static Vecr parseVecr(const QString& text) {
    Vecr v;
    const auto parts = text.split(QRegularExpression("[,;\\s]+"),
                              QString::SkipEmptyParts);
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

    chart->setTitle("Rhythmic deliveries");
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


void MainWindow::on_rhBuildPBtn_clicked()
{
    const int N = ui->rhNSpin->value();
    if (N <= 0) {
        QMessageBox::warning(this, "Ошибка", "N должно быть > 0");
        return;
    }

    auto* table = ui->rhPTable;

    table->clear();
    table->setRowCount(1);
    table->setColumnCount(N);

    // Заголовки: p[1]..p[N] (или p[0]..p[N-1] — как тебе нужно)
    QStringList headers;
    headers.reserve(N);
    for (int i = 0; i < N; ++i)
        headers << QString("p[%1]").arg(i + 1);

    table->setHorizontalHeaderLabels(headers);

    // Если хочешь подпись строки "p"
    table->setVerticalHeaderLabels(QStringList() << "p");

    table->setAlternatingRowColors(true);

    // Растягивание колонок по ширине таблицы (удобно, если N не огромный)
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->horizontalHeader()->setDefaultSectionSize(80);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    // Заполняем ячейки нулями (или оставь пустыми)
    for (int j = 0; j < N; ++j) {
        auto* it = new QTableWidgetItem("0");
        it->setTextAlignment(Qt::AlignCenter);
        table->setItem(0, j, it);
    }
}


static Vecr readPFromTable(QTableWidget* table)
{
        Vecr p;

        const int N = table->columnCount();
        if (table->rowCount() < 1 || N <= 0) return p;

        p.resize(N);

        for (int i = 0; i < N; ++i) {
            QTableWidgetItem* item = table->item(0, i);
            QString s = item ? item->text().trimmed() : "";

            if (s.isEmpty()) {
                // если хочешь разрешить пустые — поставь 0 и убери ошибку
                QMessageBox::warning(nullptr, "Ошибка",
                                     QString("Пустое значение в p[%1]").arg(i + 1));
                return Vecr{};
            }

            // поддержка запятой
            s.replace(',', '.');

            bool ok = false;
            double val = s.toDouble(&ok);
            if (!ok) {
                QMessageBox::warning(nullptr, "Ошибка",
                                     QString("Некорректное число в p[%1]: '%2'")
                                         .arg(i + 1).arg(s));
                return Vecr{};
            }

            p[i] = val;
        }
        return p;

}


void MainWindow::on_rhSolveIterBtn_clicked()
{
    const Vecr p = readPFromTable(ui->rhPTable);
    if (p.empty()) {
        QMessageBox::warning(this, "Ошибка", "Заполни таблицу p[t] (все строки должны быть числами).");
        return;
    }

    const double V0   = ui->rhV0Spin->value();
    const double minV = ui->rhMinVSpin->value();
    const double maxV = ui->rhMaxVSpin->value();

    if (minV > maxV) {
        QMessageBox::warning(this, "Ошибка", "minV не может быть больше maxV.");
        return;
    }

    const UniformityIterResult res = solve_rhythmic_delivery_uniform_pg(p, V0, minV, maxV);

    // таблица результатов
    ui->rhResultTable->clear();
    ui->rhResultTable->setColumnCount(4);
    ui->rhResultTable->setRowCount((int)p.size());
    ui->rhResultTable->setHorizontalHeaderLabels({"t", "p[t]", "x[t]", "V[t]"});

    const int n  = (int)p.size();
    const int nx = (int)res.x.size();
    const int nV = (int)res.V.size();

    for (int t = 0; t < n; ++t) {
        ui->rhResultTable->setItem(t, 0, new QTableWidgetItem(QString::number(t + 1)));
        ui->rhResultTable->setItem(t, 1, new QTableWidgetItem(QString::number(p[t])));

        ui->rhResultTable->setItem(t, 2, new QTableWidgetItem(t < nx ? QString::number(res.x[t]) : "-"));
        ui->rhResultTable->setItem(t, 3, new QTableWidgetItem(t < nV ? QString::number(res.V[t]) : "-"));
    }

    ui->rhResultTable->resizeColumnsToContents();

    const QString status =
        QString("ok=%1 | Mp=%2 | iters=%3/%4")
            .arg(res.ok ? "true" : "false")
            .arg(res.Mp)
            .arg(res.iters)
            .arg(res.maxIter);

    auto* view = makeRhythmicChart(toQVec(p), toQVec(res.x), toQVec(res.V), minV, maxV);
    if (view) {
        auto* w = new QWidget();
        w->setWindowTitle("Chart");
        auto* lay = new QVBoxLayout(w);
        lay->addWidget(view);
        w->resize(1000, 700);
        w->show();
    }
}


void MainWindow::on_rhSolveDirectBtn_clicked()
{
    const Vecr p = readPFromTable(ui->rhPTable);
    if (p.empty()) {
        QMessageBox::warning(this, "Ошибка", "Заполни таблицу p[t] (все строки должны быть числами).");
        return;
    }

    const double V0   = ui->rhV0Spin->value();
    const double minV = ui->rhMinVSpin->value();
    const double maxV = ui->rhMaxVSpin->value();

    if (minV > maxV) {
        QMessageBox::warning(this, "Ошибка", "minV не может быть больше maxV.");
        return;
    }

    const DeliveryResult res = solve_rhythmic_delivery_bounds_direct(p, V0, minV, maxV);

    ui->rhResultTable->clear();
    ui->rhResultTable->setColumnCount(4);
    ui->rhResultTable->setRowCount((int)p.size());
    ui->rhResultTable->setHorizontalHeaderLabels({"t", "p[t]", "x[t]", "V[t]"});

    const int n  = (int)p.size();
    const int nx = (int)res.x.size();
    const int nV = (int)res.V.size();

    for (int t = 0; t < n; ++t) {
        ui->rhResultTable->setItem(t, 0, new QTableWidgetItem(QString::number(t + 1)));
        ui->rhResultTable->setItem(t, 1, new QTableWidgetItem(QString::number(p[t])));
        ui->rhResultTable->setItem(t, 2, new QTableWidgetItem(t < nx ? QString::number(res.x[t]) : "-"));
        ui->rhResultTable->setItem(t, 3, new QTableWidgetItem(t < nV ? QString::number(res.V[t]) : "-"));
    }

    ui->rhResultTable->resizeColumnsToContents();

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

        QString s = it->text().trimmed();
        if (s.isEmpty()) continue;

        const auto parts = s.split(QRegularExpression("[,;\\s]+"),
                           QString::SkipEmptyParts);

        for (const auto& token : parts) {
            bool ok = false;
            int pr1 = token.toInt(&ok);     // pr1 = номер работы как ввёл пользователь (1..N)
            if (!ok) continue;

            if (pr1 == 0) {
                // удобно: "0" означает "нет предшественников"
                continue;
            }

            int pr = pr1 - 1;               // переводим в 0..N-1 для solver
            if (pr < 0 || pr >= N) {
                QMessageBox::warning(nullptr, "Ошибка",
                                     QString("Некорректный предшественник %1 в строке работы %2. Допустимо: 1..%3 (или 0).")
                                         .arg(pr1).arg(i + 1).arg(N));
                return VecVeci{}; // сигнал ошибки
            }
            preds[i].push_back(pr);
        }
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

    // -------- jobsTable: N x 2 --------
    ui->jobsTable->setRowCount(N);
    ui->jobsTable->setColumnCount(2);
    ui->jobsTable->setHorizontalHeaderLabels({"dur", "rel"});

    // Нумерация работ 1..N слева
    QStringList jobRow;
    for (int i = 0; i < N; ++i) jobRow << QString::number(i + 1);
    ui->jobsTable->setVerticalHeaderLabels(jobRow);

    // -------- capTable: 1 x M --------
    ui->capTable->setRowCount(1);
    ui->capTable->setColumnCount(M);

    QStringList capHeaders;
    for (int m = 0; m < M; ++m) capHeaders << QString("R%1").arg(m + 1); // R1..RM
    ui->capTable->setHorizontalHeaderLabels(capHeaders);

    ui->capTable->setVerticalHeaderLabels(QStringList() << "cap"); // опционально

    // -------- demandsTable: N x M --------
    ui->demandsTable->setRowCount(N);
    ui->demandsTable->setColumnCount(M);
    ui->demandsTable->setHorizontalHeaderLabels(capHeaders);
    ui->demandsTable->setVerticalHeaderLabels(jobRow); // работы 1..N

    // -------- predsTable: N x 1 --------
    ui->predsTable->setRowCount(N);
    ui->predsTable->setColumnCount(1);
    ui->predsTable->setHorizontalHeaderLabels({"preds (enter job numbers starting from 1, separated by spaces)"});
    ui->predsTable->setVerticalHeaderLabels(jobRow);

    // -------- scheduleTable: N x 3 --------
    ui->scheduleTable->setRowCount(N);
    ui->scheduleTable->setColumnCount(3);
    ui->scheduleTable->setHorizontalHeaderLabels({"job", "start", "finish"});
    ui->scheduleTable->setVerticalHeaderLabels(jobRow);

    niceTable(ui->jobsTable);
    niceTable(ui->capTable);
    niceTable(ui->demandsTable);
    niceTable(ui->predsTable);
    niceTable(ui->scheduleTable);
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
        ui->scheduleTable->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        ui->scheduleTable->setItem(i, 1, new QTableWidgetItem(i < (int)s.start.size() ? QString::number(s.start[i]) : "-"));
        ui->scheduleTable->setItem(i, 2, new QTableWidgetItem(i < (int)s.finish.size() ? QString::number(s.finish[i]) : "-"));
    }

    // ui->zkprStatusLabel->setText(QString("Готово. cmax=%1").arg(s.cmax));
    showGanttChartWindow(this, s.start, s.finish, preds, s.cmax);
}


void MainWindow::on_rhTestBtn_clicked()
{
    // Тестовые данные
    const int N = 12;
    const std::vector<double> p = {
        119.36, 123.86, 150.51, 162.69, 160.00, 158.05,
        161.61, 149.99, 195.13, 188.69, 146.61, 143.46
    };

    const double V0   = 128.81;
    const double minV = 55.0;
    const double maxV = 220.0;

    // 1) выставляем параметры
    ui->rhNSpin->setValue(N);
    ui->rhV0Spin->setValue(V0);
    ui->rhMinVSpin->setValue(minV);
    ui->rhMaxVSpin->setValue(maxV);

    // 2) строим таблицу p на N столбцов (используем твою кнопку "построить")
    on_rhBuildPBtn_clicked();

    // 3) заполняем строку p
    for (int j = 0; j < N; ++j) {
        auto* it = ui->rhPTable->item(0, j);
        if (!it) {
            it = new QTableWidgetItem();
            ui->rhPTable->setItem(0, j, it);
        }
        it->setText(QString::number(p[j], 'f', 2)); // 2 знака после запятой
        it->setTextAlignment(Qt::AlignCenter);
    }

    // опционально: сразу решить (раскомментируй что нужно)
    // on_rhSolveIterBtn_clicked();
    // on_rhSolveDirectBtn_clicked();
}





void MainWindow::on_zkprTestBtn_clicked()
{
    const int N = 10;
    const int M = 6;

    // dur/rel/cap
    const int dur[N] = {1, 3, 5, 2, 2, 1, 1, 3, 5, 2};
    const int rel[N] = {1, 1, 1, 1, 4, 4, 4, 4, 4, 4};
    const int cap[M] = {1, 1, 1, 1, 1, 1};

    // demands: job i -> (resourceIndex 0..M-1, amount)
    // заполним как 1 в нужном ресурсе
    // job1 R1, job2 R2, job3 R3, job4 R4, job5 R5, job6 R6, job7 R1, job8 R2, job9 R3, job10 R4
    const int demJobRes[N] = {0, 1, 2, 3, 4, 5, 0, 1, 2, 3}; // 0-based ресурсы

    // preds (у тебя в примере 0-based) -> для ввода в таблицу сделаем 1-based строку
    // job1: []
    // job2: [1]
    // job3: [1]
    // job4: [2,3]
    // job5: []
    // job6: [5]
    // job7: [1]
    // job8: [7,2]
    // job9: [7,3]
    // job10:[8,9,4]
    const char* predsStr[N] = {
        "",        // 1
        "1",       // 2
        "1",       // 3
        "2 3",     // 4
        "",        // 5
        "5",       // 6
        "1",       // 7
        "7 2",     // 8
        "7 3",     // 9
        "8 9 4"    // 10
    };

    // 1) выставляем N и M
    ui->nSpin->setValue(N);
    ui->mSpin->setValue(M);

    // 2) строим таблицы нужного размера
    on_buildZkprBtn_clicked();

    // helper: записать число в ячейку
    auto setCellInt = [](QTableWidget* t, int r, int c, int v) {
        auto* it = t->item(r, c);
        if (!it) { it = new QTableWidgetItem(); t->setItem(r, c, it); }
        it->setText(QString::number(v));
        it->setTextAlignment(Qt::AlignCenter);
    };

    // 3) jobsTable: dur/rel
    for (int i = 0; i < N; ++i) {
        setCellInt(ui->jobsTable, i, 0, dur[i]);
        setCellInt(ui->jobsTable, i, 1, rel[i]);
    }

    // 4) capTable: 1 x M
    for (int m = 0; m < M; ++m)
        setCellInt(ui->capTable, 0, m, cap[m]);

    // 5) demandsTable: N x M (всё 0, и по одному ресурсу = 1)
    for (int i = 0; i < N; ++i) {
        for (int m = 0; m < M; ++m)
            setCellInt(ui->demandsTable, i, m, 0);

        setCellInt(ui->demandsTable, i, demJobRes[i], 1);
    }

    // 6) predsTable: строка с предшественниками (1-based)
    for (int i = 0; i < N; ++i) {
        auto* it = ui->predsTable->item(i, 0);
        if (!it) { it = new QTableWidgetItem(); ui->predsTable->setItem(i, 0, it); }
        it->setText(predsStr[i]);
    }
}

