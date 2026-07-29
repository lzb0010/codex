# ROS2 控制 STM32F407 开灯/关灯 Demo 文档

适用于：Ubuntu 虚拟机 + ROS2 + VSCode + USB-TTL/CH340 + STM32F407 串口通信。

## 1. Demo 目标

本 Demo 的目标是用 ROS2 的 Topic 话题通信发送“开灯/关灯”指令，再由串口节点把指令转换成 STM32F407 可以识别的串口命令。

| 模块                   | 作用                                    |
| -------------------- | ------------------------------------- |
| `send_action` 节点     | 发布中文动作指令，例如“开灯”“关灯”                   |
| `/cmd_action` 话题     | 连接发布者和订阅者，消息类型为 `std_msgs/msg/String` |
| `serial_to_stm32` 节点 | 订阅动作指令，并通过串口发送 `LED_ON` 或 `LED_OFF`   |
| STM32F407            | 通过 USART 接收字符串，解析后控制 GPIO             |

通信链路：

```text
键盘输入
  -> ROS2 发布节点 send_action
  -> Topic: /cmd_action
  -> ROS2 订阅节点 serial_to_stm32
  -> /dev/ttyUSB0
  -> USB-TTL
  -> STM32F407 USART
  -> GPIO 控制 LED
```

ROS2 Topic 通信成立的关键条件是：

```text
话题名一致 + 消息类型一致
```

本 Demo 中：

```text
话题名：/cmd_action
消息类型：std_msgs/msg/String
```

## 2. 串口协议

| ROS2 指令 | 串口发送内容      | STM32 动作 |
| ------- | ----------- | -------- |
| 开灯      | `LED_ON\n`  | 点亮 LED   |
| 关灯      | `LED_OFF\n` | 熄灭 LED   |

末尾的 `\n` 很重要，它表示一条串口命令结束。STM32 端可以一直接收字符，遇到换行符后再统一解析。

## 3. 创建 ROS2 工作空间和功能包

```bash
source /opt/ros/humble/setup.bash

mkdir -p ~/ros2_ws/src
cd ~/ros2_ws/src

ros2 pkg create stm32_demo --build-type ament_python --dependencies rclpy std_msgs

cd ~/ros2_ws
colcon build
source install/setup.bash

code .
```

创建后的主要目录：

```text
~/ros2_ws/
  src/
    stm32_demo/
      setup.py
      package.xml
      stm32_demo/
        __init__.py
```

## 4. 发布节点：send_action.py

文件路径：

```text
~/ros2_ws/src/stm32_demo/stm32_demo/send_action.py
```

代码：

```python
import rclpy
from rclpy.node import Node
from std_msgs.msg import String


class SendActionNode(Node):
    def __init__(self):
        super().__init__("send_action_node")
        self.publisher = self.create_publisher(String, "/cmd_action", 10)

    def send_command(self, text):
        msg = String()
        msg.data = text
        self.publisher.publish(msg)
        self.get_logger().info(f"已发布指令: {text}")


def main(args=None):
    rclpy.init(args=args)
    node = SendActionNode()

    try:
        while rclpy.ok():
            text = input("请输入动作指令，例如 开灯 / 关灯：")
            node.send_command(text)
    except KeyboardInterrupt:
        pass

    node.destroy_node()
    rclpy.shutdown()


if __name__ == "__main__":
    main()
```

关键函数：

| 函数                                            | 作用          |
| --------------------------------------------- | ----------- |
| `create_publisher(String, "/cmd_action", 10)` | 创建发布者       |
| `self.publisher.publish(msg)`                 | 发布消息到 Topic |

## 5. 串口节点：serial_to_stm32.py

文件路径：

```text
~/ros2_ws/src/stm32_demo/stm32_demo/serial_to_stm32.py
```

代码：

```python
import rclpy
from rclpy.node import Node
from std_msgs.msg import String
import serial


class SerialToSTM32Node(Node):
    def __init__(self):
        super().__init__("serial_to_stm32_node")

        self.declare_parameter("port", "/dev/ttyUSB0")
        self.declare_parameter("baud", 115200)

        self.serial_port = self.get_parameter("port").value
        self.baud_rate = self.get_parameter("baud").value

        self.ser = serial.Serial(self.serial_port, self.baud_rate, timeout=1)

        self.subscription = self.create_subscription(
            String,
            "/cmd_action",
            self.cmd_callback,
            10
        )

        self.get_logger().info(f"串口已打开: {self.serial_port}")

    def cmd_callback(self, msg):
        cmd = msg.data

        if cmd == "开灯":
            data = "LED_ON\n"
        elif cmd == "关灯":
            data = "LED_OFF\n"
        else:
            self.get_logger().warn(f"未知指令: {cmd}")
            return

        self.ser.write(data.encode("utf-8"))
        self.get_logger().info(f"已发送到 STM32: {data.strip()}")

    def destroy_node(self):
        if hasattr(self, "ser") and self.ser.is_open:
            self.ser.close()
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = SerialToSTM32Node()

    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass

    node.destroy_node()
    rclpy.shutdown()


if __name__ == "__main__":
    main()
```

关键函数：

| 函数                                                                  | 作用            |
| ------------------------------------------------------------------- | ------------- |
| `create_subscription(String, "/cmd_action", self.cmd_callback, 10)` | 创建订阅者         |
| `cmd_callback(self, msg)`                                           | 收到消息后自动执行     |
| `serial.Serial(...)`                                                | 打开 Linux 串口   |
| `self.ser.write(...)`                                               | 通过串口发送给 STM32 |
| `rclpy.spin(node)`                                                  | 让节点持续运行和监听    |

## 6. 修改 setup.py

打开：

```text
~/ros2_ws/src/stm32_demo/setup.py
```

把 `entry_points` 改成：

```python
entry_points={
    'console_scripts': [
        'send_action = stm32_demo.send_action:main',
        'serial_to_stm32 = stm32_demo.serial_to_stm32:main',
    ],
},
```

如果不改这一步，`ros2 run stm32_demo serial_to_stm32` 会找不到节点。

## 7. 编译和运行

安装串口库：

```bash
sudo apt install python3-serial
```

编译：

```bash
cd ~/ros2_ws
colcon build --packages-select stm32_demo
source install/setup.bash
```

运行串口节点：

```bash
cd ~/ros2_ws
source install/setup.bash
ros2 run stm32_demo serial_to_stm32 --ros-args -p port:=/dev/ttyUSB0
```

运行发布节点：

```bash
cd ~/ros2_ws
source install/setup.bash
ros2 run stm32_demo send_action
```

然后输入：

```text
开灯
```

串口节点会发送：

```text
LED_ON
```

也可以不运行 `send_action`，直接命令行发布：

```bash
ros2 topic pub /cmd_action std_msgs/msg/String "{data: '开灯'}"
```

## 8. ROS2 Topic 常用语法

查看话题：

```bash
ros2 topic list
```

查看话题数据：

```bash
ros2 topic echo /cmd_action
```

查看话题类型：

```bash
ros2 topic type /cmd_action
```

查看消息结构：

```bash
ros2 interface show std_msgs/msg/String
```

手动发布：

```bash
ros2 topic pub /cmd_action std_msgs/msg/String "{data: '开灯'}"
```

## 9. STM32F407 端参考代码

下面是 HAL 中断接收字符串的基本思路，实际 LED 引脚请按你的开发板修改。

#### ros_serival.c

```c

#include "main.h"
#include <string.h>
#include "usart.h"
extern UART_HandleTypeDef huart1;

uint8_t rx_byte;
char rx_buffer[64];
uint8_t rx_index = 0;

void UART_Start_Receive(void)
{
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

void handle_command(char *cmd)
{
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
    if (strcmp(cmd, "LED_ON") == 0)
    {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
    }
    else if (strcmp(cmd, "LED_OFF") == 0)
    {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
    }
    else if (strcmp(cmd, "MOTOR_FORWARD") == 0)
    {
        // 开启电机正转，例如设置方向 GPIO + PWM
    }
    else if (strcmp(cmd, "MOTOR_STOP") == 0)
    {
        // 停止 PWM 或关闭电机驱动
    }
}


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        if (rx_byte == '\n')
        {
            rx_buffer[rx_index] = '\0';
            handle_command(rx_buffer);
            rx_index = 0;
        }
        else
        {
            if (rx_index < sizeof(rx_buffer) - 1)
            {
                rx_buffer[rx_index++] = rx_byte;
            }
            else
            {
                rx_index = 0;
            }
        }





  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}


```

#### ros_serival.h

```c
#ifndef __ROS_SERIVAL_H__
#define __ROS_SERIVAL_H__

void UART_Start_Receive(void);
	
void handle_command(char *cmd);

#endif

```



在 `main()` 中初始化 GPIO、USART 后调用：

```c
UART_Start_Receive();
```

如果你的 LED 是高电平点亮，就把 `SET` / `RESET` 逻辑反过来。

## 10. USB-TTL 接线

| USB-TTL | STM32F407        | 说明                   |
| ------- | ---------------- | -------------------- |
| TXD     | PA10 / USART1_RX | USB-TTL 发，STM32 收    |
| RXD     | PA9 / USART1_TX  | STM32 发，USB-TTL 收    |
| GND     | GND              | 必须共地                 |
| 3.3V    | 通常不接             | 除非明确要用 USB-TTL 给板子供电 |

注意：STM32F407 是 3.3V TTL 电平，不要直接接 5V 串口电平。

## 11. 串口排查：CH340 被 brltty 抢占

你遇到的现象是：

```bash
lsusb
```

能看到：

```text
ID 1a86:7523 QinHeng Electronics CH340 serial converter
```

但是：

```bash
ls /dev/ttyUSB*
```

没有 `/dev/ttyUSB0`。

你的日志里有：

```text
ch341-uart converter now attached to ttyUSB0
brltty sets config #1
ch341-uart converter now disconnected from ttyUSB0
```

这说明 `ttyUSB0` 本来已经生成了，但是被 `brltty` 抢占后断开。

临时解决：

```bash
sudo systemctl stop brltty
```

然后拔插 USB-TTL，再查：

```bash
ls /dev/ttyUSB*
```

如果正常，应该出现：

```text
/dev/ttyUSB0
```

长期解决：

```bash
sudo systemctl disable brltty
sudo systemctl mask brltty
```

或者不需要盲文终端时卸载：

```bash
sudo apt remove brltty
sudo reboot
```

## 12. 串口权限

如果 `/dev/ttyUSB0` 出现了，但是运行 ROS2 报：

```text
Permission denied
```

执行：

```bash
sudo usermod -aG dialout $USER
```

然后注销或重启 Ubuntu。

检查：

```bash
groups
```

里面应该有：

```text
dialout
```

## 13. 常用检查命令

| 命令                               | 用途                           |
| -------------------------------- | ---------------------------- |
| `lsusb`                          | 查看 Ubuntu 是否识别到 USB 设备       |
| `ls /dev/ttyUSB*`                | 查看 CH340/CP2102 等 USB-TTL 串口 |
| `ls /dev/ttyACM*`                | 查看 ST-Link VCP 或 USB CDC 串口  |
| `sudo dmesg \| tail -80`         | 查看串口绑定、断开、抢占等日志              |
| `lsmod \| grep ch341`            | 查看 CH340 驱动模块是否加载            |
| `sudo usermod -aG dialout $USER` | 把当前用户加入串口权限组                 |
| `ros2 topic list`                | 查看 ROS2 话题                   |
| `ros2 topic echo /cmd_action`    | 监听动作指令                       |

## 14. 推荐扩展

开灯/关灯跑通后，可以把串口协议扩展成更通用的格式：

```text
LED,ON,0
LED,OFF,0
MOTOR,FORWARD,80
SERVO,ANGLE,90
```

这样后续接电机、舵机、继电器、传感器时，不需要重写 ROS2 通信结构，只需要扩展指令映射和 STM32 解析函数。

## 15. 最小结论

这个 Demo 的核心就是：

```text
ROS2 Topic 发布动作
  -> 订阅节点收到动作
  -> 转成串口字符串
  -> STM32F407 解析字符串
  -> 控制 GPIO
```

最关键的 ROS2 函数是：

```python
create_publisher()
publish()
create_subscription()
callback()
rclpy.spin()
```
