#include "window.hpp"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    QWidget *window = new QWidget(this);
    setCentralWidget(window);
}
