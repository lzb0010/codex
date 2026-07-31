from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        Node(
            package='stm32_demo',
            executable='serial_to_stm32',
            name='serial_to_stm32',
            output='screen'
        ),

        Node(
            package='stm32_demo',
            executable='tcp_to_ros',
            name='tcp_to_ros',
            output='screen'
        ),
    ])