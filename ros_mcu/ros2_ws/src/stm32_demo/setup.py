from setuptools import find_packages, setup

package_name = 'stm32_demo'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='huanyu',
    maintainer_email='huanyu@todo.todo',
    description='TODO: Package description',
    license='TODO: License declaration',
    extras_require={
        'test': [
            'pytest',
        ],
    },
    entry_points={
        'console_scripts': [
            'send_action = stm32_demo.send_action:main',
            'serial_to_stm32 = stm32_demo.serial_to_stm32:main',
        ],
    },
)
