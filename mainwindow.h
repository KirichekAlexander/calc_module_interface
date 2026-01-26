#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:

    void on_solveBtn_clicked();

    void on_solveDirectBtn_clicked();

    void on_buildZkprBtn_clicked();

    void on_solveZkprBtn_clicked();

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
