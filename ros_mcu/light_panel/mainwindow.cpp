#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    socket = new QTcpSocket(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_clicked()
{
    // 未连接时，点击按钮再连接 Ubuntu
    if (socket->state() != QAbstractSocket::ConnectedState) {
        socket->connectToHost("192.168.3.66", 5000);

        if (!socket->waitForConnected(2000)) {
            ui->label->setText("connect failed");
            return;
        }
    }

    socket->write("LED_ON\n");

    if (!socket->waitForBytesWritten(2000)) {
        ui->label->setText("send failed");
        return;
    }

    ui->label->setText("state:on");
}
