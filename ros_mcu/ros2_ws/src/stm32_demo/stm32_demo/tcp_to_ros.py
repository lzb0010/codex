import socketserver
import threading

import rclpy
from rclpy.node import Node
from std_msgs.msg import String


class TcpHandler(socketserver.StreamRequestHandler):
    def handle(self):
        node = self.server.ros_node
        client_ip = self.client_address[0]
        node.get_logger().info(f"i.MX6ULL 已连接: {client_ip}")

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
                node.get_logger().warn(f"未知网络命令: {cmd}")
                continue

            msg = String()
            msg.data = ros_cmd
            node.publisher.publish(msg)
            node.get_logger().info(f"已发布 /cmd_action: {ros_cmd}")

            self.wfile.write(b"OK\n")


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

        self.get_logger().info("TCP 服务已启动，监听端口 5000")

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