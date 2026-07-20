import os
import select
import termios
import time


fd = os.open("/dev/ttyUSB0", os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
old = termios.tcgetattr(fd)
new = termios.tcgetattr(fd)
new[0] = 0
new[1] = 0
new[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
new[3] = 0
new[4] = termios.B115200
new[5] = termios.B115200
new[6][termios.VMIN] = 0
new[6][termios.VTIME] = 1
termios.tcsetattr(fd, termios.TCSANOW, new)


def collect(seconds=1.5):
    end = time.time() + seconds
    output = b""
    while time.time() < end:
        ready, _, _ = select.select([fd], [], [], 0.15)
        if ready:
            try:
                output += os.read(fd, 4096)
            except BlockingIOError:
                pass
    return output.decode("utf-8", "replace")


def run(command, seconds=1.5):
    print("\n### CMD:", command)
    os.write(fd, command.encode() + b"\r")
    time.sleep(0.25)
    print(collect(seconds), end="")


try:
    os.write(fd, b"\r")
    time.sleep(0.2)
    print(collect(0.6), end="")
    run("[ -e /dev/chrevbase ] || mknod /dev/chrevbase c 200 0; ls -l /dev/chrevbase")
    run("printf 'read_result='; cat /dev/chrevbase; echo")
    run("if printf 'board-write-test' > /dev/chrevbase; then echo write_result=success; else echo write_result=failed; fi")
    run("dmesg | tail -n 12", 2)
finally:
    termios.tcsetattr(fd, termios.TCSANOW, old)
    os.close(fd)
