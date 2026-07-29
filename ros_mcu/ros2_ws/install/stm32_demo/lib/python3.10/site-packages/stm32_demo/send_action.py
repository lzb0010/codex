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
            text = input("请输入动作指令，例如 开灯 / 关灯 / 前进 / 停止：")
            node.send_command(text)
    except KeyboardInterrupt:
        pass

    node.destroy_node()
    rclpy.shutdown()


if __name__ == "__main__":
    main()