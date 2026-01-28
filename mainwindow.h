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

    void on_buildZkprBtn_clicked();

    void on_solveZkprBtn_clicked();

    void on_rhBuildPBtn_clicked();
    void on_rhSolveIterBtn_clicked();
    void on_rhSolveDirectBtn_clicked();

    void on_rhTestBtn_clicked();

    void on_zkprTestBtn_clicked();

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
