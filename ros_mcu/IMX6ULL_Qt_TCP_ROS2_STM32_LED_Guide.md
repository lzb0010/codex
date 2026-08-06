# i.MX6ULL Qt 屏幕通过 Ubuntu ROS 2 控制 STM32 LED

## 1. 目标和通信链路

已有的 Ubuntu 端功能：

```text
ROS 2 话题 /cmd_action
  -> serial_to_stm32 节点
  -> /dev/ttyUSB0
  -> STM32F407
  -> LED
```

本次增加 i.MX6ULL Qt 屏幕控制。i.MX6ULL 不安装 ROS 2，也不操作串口；它仅通过局域网 TCP 向 Ubuntu 发送命令。

```text
i.MX6ULL Qt 按钮
  -> TCP：Ubuntu IP:5000
  -> Ubuntu tcp_to_ros 节点
  -> ROS 2 话题 /cmd_action
  -> serial_to_stm32 节点
  -> STM32F407 串口
  -> LED
```

使用的网络命令：

| Qt 发送内容 | Ubuntu 发布到 `/cmd_action` | STM32 最终收到的串口内容 |
| --- | --- | --- |
| `LED_ON\n` | `开灯` | `LED_ON\n` |
| `LED_OFF\n` | `关灯` | `LED_OFF\n` |

> `\n` 是换行符，表示一条命令结束，不能省略。

---

## 2. Ubuntu：保留原有串口控制节点

原有的 `serial_to_stm32` 不需要修改。它继续订阅：

```text
/cmd_action
```

并把“开灯”和“关灯”转换为 `LED_ON\n`、`LED_OFF\n` 发给 STM32。

运行：

```bash
source ~/ros2_ws/install/setup.bash
ros2 run stm32_demo serial_to_stm32
```

---

## 3. Ubuntu：创建 TCP 转 ROS 2 节点

在 Ubuntu 创建文件：

```text
~/ros2_ws/src/stm32_demo/stm32_demo/tcp_to_ros.py
```

文件内容：

```python
import socketserver
import threading

import rclpy
from rclpy.node import Node
from std_msgs.msg import String


class TcpHandler(socketserver.StreamRequestHandler):
    def handle(self):
        node = self.server.ros_node
        client_ip = self.client_address[0]
        node.get_logger().info(f"i.MX6ULL connected: {client_ip}")

        while rclpy.ok():
            data = self.rfile.readline()
            if not data:
                break

            cmd = data.decode("utf-8").strip()

            if cmd == "LED_ON":
                ros_cmd = "开灯"
            elif cmd == "LED_OFF":
                ros_cmd = "关灯"
            else:
                node.get_logger().warn(f"unknown command: {cmd}")
                continue

            msg = String()
            msg.data = ros_cmd
            node.publisher.publish(msg)
            node.get_logger().info(f"published /cmd_action: {ros_cmd}")

            self.wfile.write(b"OK\\n")


class TcpToRosNode(Node):
    def __init__(self):
        super().__init__("tcp_to_ros_node")
        self.publisher = self.create_publisher(String, "/cmd_action", 10)

        self.server = socketserver.ThreadingTCPServer(
            ("0.0.0.0", 5000), TcpHandler
        )
        self.server.ros_node = self
        self.server.daemon_threads = True

        self.thread = threading.Thread(
            target=self.server.serve_forever,
            daemon=True
        )
        self.thread.start()

        self.get_logger().info("TCP service started, port: 5000")

    def destroy_node(self):
        self.server.shutdown()
        self.server.server_close()
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = TcpToRosNode()

    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass

    node.destroy_node()
    rclpy.shutdown()


if __name__ == "__main__":
    main()
```

### 修改 `setup.py`

打开：

```text
~/ros2_ws/src/stm32_demo/setup.py
```

在 `console_scripts` 中加入：

```python
'tcp_to_ros = stm32_demo.tcp_to_ros:main',
```

例如：

```python
entry_points={
    'console_scripts': [
        'send_action = stm32_demo.send_action:main',
        'serial_to_stm32 = stm32_demo.serial_to_stm32:main',
        'tcp_to_ros = stm32_demo.tcp_to_ros:main',
    ],
},
```

重新编译：

```bash
cd ~/ros2_ws
colcon build --packages-select stm32_demo
source install/setup.bash
```

运行 TCP 服务：

```bash
ros2 run stm32_demo tcp_to_ros
```

看到以下提示表示 Ubuntu 已经等待 i.MX6ULL 连接：

```text
TCP service started, port: 5000
```

---

## 4. 先在 Ubuntu 本机验证 TCP 服务

新开一个 Ubuntu 终端，监听话题：

```bash
source ~/ros2_ws/install/setup.bash
ros2 topic echo /cmd_action
```

再新开一个终端，发送测试命令：

```bash
printf 'LED_ON\n' > /dev/tcp/127.0.0.1/5000
```

正常时，话题终端显示：

```text
data: 开灯
```

如果原来的 `serial_to_stm32` 正在运行，STM32 的 LED 也会点亮。

---

## 5. i.MX6ULL Qt 工程修改

假设工程名为 `light_panel`。

### 5.1 修改 `light_panel.pro`

加入网络模块：

```pro
QT += widgets network
```

修改 `.pro` 后，必须重新执行 `qmake` 再编译。

### 5.2 修改 `mainwindow.h`

加入头文件：

```cpp
#include <QTcpSocket>
```

在 `private:` 中加入成员变量：

```cpp
QTcpSocket *socket;
```

关键部分示例：

```cpp
private:
    Ui::MainWindow *ui;
    QTcpSocket *socket;
```

### 5.3 修改 `mainwindow.cpp`

下面是完整的开灯按钮示例。将 `192.168.3.66` 改为 Ubuntu 的实际局域网 IP。

```cpp
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
    // 未连接时，连接 Ubuntu 的 TCP 服务
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
```

说明：每点击一次 `pushButton`，都会发送一次 `LED_ON\n`，即执行一次开灯命令。

### 5.4 关灯按钮

如果第二个按钮对象名为 `pushButton_2`，可添加：

```cpp
void MainWindow::on_pushButton_2_clicked()
{
    if (socket->state() != QAbstractSocket::ConnectedState) {
        socket->connectToHost("192.168.3.66", 5000);

        if (!socket->waitForConnected(2000)) {
            ui->label->setText("connect failed");
            return;
        }
    }

    socket->write("LED_OFF\n");

    if (!socket->waitForBytesWritten(2000)) {
        ui->label->setText("send failed");
        return;
    }

    ui->label->setText("state:off");
}
```

> 如果你的按钮对象名不是 `pushButton` 或 `pushButton_2`，必须把函数名改成对应形式，例如对象名是 `btnOn`，函数应为 `on_btnOn_clicked()`。

---

## 6. 编译与部署到 i.MX6ULL

在 Ubuntu 主机中，使用已有的 i.MX6ULL 交叉编译 qmake：

```bash
cd /home/huanyu/light_panel
rm -rf build-imx6ull
mkdir build-imx6ull
cd build-imx6ull

/home/huanyu/linux/qt5.6.3-imx6ull-build/bin/qmake ..
make -j4
```

复制到 NFS 根文件系统：

```bash
mkdir -p /home/huanyu/linux/rootfs_gst/opt/light_panel
cp light_panel /home/huanyu/linux/rootfs_gst/opt/light_panel/
chmod +x /home/huanyu/linux/rootfs_gst/opt/light_panel/light_panel
```

在开发板运行：

```bash
/opt/light_panel/light_panel
```

---

## 7. 排错顺序

### 7.1 Qt 标签不变化

`on_pushButton_clicked()` 没有被执行。检查 Qt Designer 中按钮的 `objectName` 是否为：

```text
pushButton
```

若对象名不同，槽函数名也必须相应修改。

### 7.2 Qt 显示 `connect failed`

说明 i.MX6ULL 无法连接到 Ubuntu 的 `192.168.3.66:5000`。

在 i.MX6ULL 上测试网络：

```bash
ping -c 3 192.168.3.66
```

在 Ubuntu 上确认 5000 端口监听：

```bash
ss -ltn | grep 5000
```

正常应看到：

```text
LISTEN ... 0.0.0.0:5000
```

若 Ubuntu 开启 UFW 防火墙：

```bash
sudo ufw allow 5000/tcp
```

### 7.3 Qt 显示 `state:on`，但 LED 不亮

说明 i.MX6ULL 已经向 Ubuntu 发送成功。按顺序检查：

1. `ros2 run stm32_demo tcp_to_ros` 是否仍在运行；
2. `ros2 run stm32_demo serial_to_stm32` 是否仍在运行；
3. `ros2 topic echo /cmd_action` 是否出现 `data: 开灯`；
4. Ubuntu 串口是否仍为正确的 `/dev/ttyUSB0`；
5. STM32 是否能正确接收 `LED_ON\n`。

---

## 8. 日常运行顺序

Ubuntu 终端 1：

```bash
source ~/ros2_ws/install/setup.bash
ros2 run stm32_demo serial_to_stm32
```

Ubuntu 终端 2：

```bash
source ~/ros2_ws/install/setup.bash
ros2 run stm32_demo tcp_to_ros
```

最后，在 i.MX6ULL 上启动 Qt 程序并点击开灯、关灯按钮。
