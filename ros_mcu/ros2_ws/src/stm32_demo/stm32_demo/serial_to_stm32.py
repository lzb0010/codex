import rclpy
from rclpy.node import Node
from std_msgs.msg import String
import serial


class SerialToSTM32Node(Node):
    def __init__(self):
        super().__init__("serial_to_stm32_node")

        self.serial_port = "/dev/ttyUSB0"
        self.baud_rate = 115200

        self.ser = serial.Serial(
            self.serial_port,
            self.baud_rate,
            timeout=1
        )

        self.subscription = self.create_subscription(
            String,
            "/cmd_action",
            self.cmd_callback,
            10
        )


        self.led_state_pub = self.create_publisher(
            String,
            '/led_state',
            10
        )

        self.create_timer(0.05, self.read_serial_callback)

        self.get_logger().info(f"串口已打开: {self.serial_port}")

    def cmd_callback(self, msg):
        cmd = msg.data

        if cmd == "开灯":
            data = "LED_ON\n"
        elif cmd == "关灯":
            data = "LED_OFF\n"
        elif cmd == "前进":
            data = "MOTOR_FORWARD\n"
        elif cmd == "停止":
            data = "MOTOR_STOP\n"
        else:
            self.get_logger().warn(f"未知指令: {cmd}")
            return

        self.ser.write(data.encode("utf-8"))
        self.get_logger().info(f"已发送到 STM32: {data.strip()}")

    def read_serial_callback(self):
        while self.ser.in_waiting > 0:
            line = self.ser.readline().decode(
                'utf-8', errors='ignore'
            ).strip()

            if line in ('LED:ON', 'LED:OFF'):
                state_msg = String()
                state_msg.data = 'ON' if line == 'LED:ON' else 'OFF'

                self.led_state_pub.publish(state_msg)
                self.get_logger().info(
                    f'LED state from STM32: {state_msg.data}'
                )
                
    def destroy_node(self):
        if self.ser.is_open:
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