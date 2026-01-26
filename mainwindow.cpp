#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QRegularExpression>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QHeaderView>
#include "rhythmic_delivery.h"
#include "pcplp.h"

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
}


