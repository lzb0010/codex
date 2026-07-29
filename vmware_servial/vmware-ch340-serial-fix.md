# VMware Ubuntu CH340 Serial Troubleshooting

Date: 2026-07-26,vmware识别串口识别，核心原因是brltty 抢占 CH340，

直接解决方法：

sudo systemctl stop brltty

sudo systemctl disable brltty

sudo systemctl mask brltty

## Symptoms

- Windows Device Manager showed a warning on `USB Serial`.
- VMware reported:

```text
serial1: cannot open serial port "COM3": semaphore timeout period has expired.
Cannot connect virtual device "serial1".
```

- VMware also reported:

```text
serial0: parameter "serial0.fileType" has invalid value "thinprint".
Virtual device "serial0" will disconnect.
```

- Ubuntu could not find the serial device:

```bash
ls /dev/ttyACM*
# No such file or directory
```

## Root Cause

There were three separate issues mixed together:

1. The real USB serial adapter was a CH340 device on Windows:

```text
USB-SERIAL CH340 (COM6)
USB\VID_1A86&PID_7523
```

2. VMware `serial1` was configured to use `COM3`, but `COM3` was a Bluetooth serial port, not the CH340 adapter.

3. The VMX file had an invalid legacy ThinPrint serial device:

```text
serial0.fileType = "thinprint"
serial0.fileName = "thinprint"
serial0.present = "TRUE"
```

This caused VMware serial-device errors and hid/confused the removable USB workflow. The correct approach for this USB serial adapter is USB passthrough, not mapping a VMware virtual serial port to a Windows COM port.

## Fix Applied

The broken Windows PnP device instance was removed and hardware was rescanned. After that Windows detected the actual adapter correctly:

```text
USB-SERIAL CH340 (COM6)  Status: OK
```

The VM configuration file was backed up:

```text
F:\VMubantu\VM-Huanyu-ubuntu18-Melodic\VM-ubuntu18\Ubuntu 64 位.vmx.codex-bak-20260726233800
```

Then the VMX serial configuration was cleaned up:

```text
serial0.present = "FALSE"
serial1.present = "FALSE"
```

The invalid ThinPrint lines were removed:

```text
serial0.fileType = "thinprint"
serial0.fileName = "thinprint"
```

USB controllers were kept enabled:

```text
usb.present = "TRUE"
ehci.present = "TRUE"
usb_xhci.present = "TRUE"
usb.generic.allowHID = "TRUE"
```

After restarting the VM, the CH340 adapter was connected through VMware:

```text
VM -> Removable Devices -> USB-SERIAL CH340 -> Connect
```

## Ubuntu Checks

Inside Ubuntu, CH340 usually appears as `/dev/ttyUSB0`, not `/dev/ttyACM0`.

Use:

```bash
lsusb | grep -Ei '1a86|7523|ch340|qin|serial'
ls -l /dev/ttyUSB*
```

If permission is denied:

```bash
sudo usermod -aG dialout "$USER"
```

Then log out and log back in.

## Notes

- `COM3` was a Bluetooth serial port and should not be used for this adapter.
- `COM6` was the Windows-side CH340 port, but the preferred VMware setup is USB passthrough.
- In Ubuntu, ROS serial tools should use `/dev/ttyUSB0` or the actual device shown by `ls /dev/ttyUSB*`.
