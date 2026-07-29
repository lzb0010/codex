# i.MX6ULL GT9147 Touchscreen and Qt Touch Setup

This document records the confirmed touchscreen setup for the i.MX6ULL board
used with the `rootfs_gst` Qt 5.6.3 runtime.

## Confirmed Hardware

The touchscreen controller is Goodix GT9147/GT9xx, not FT5426.

- I2C controller: I2C2 (Linux bus `i2c-1`)
- I2C address: `0x14`
- Interrupt pin: `GPIO1_IO09`
- Reset pin: `GPIO5_IO09`
- Qt platform: `linuxfb`

Use this command on the board to verify the I2C address:

```sh
i2cdetect -r -y 1
```

Expected result: address `14` is present. `UU` at `1a` is the WM8960 audio
codec already claimed by its kernel driver.

## Device Tree Configuration

Edit the board DTS file in VS Code. Enable only the Goodix node below under
`&i2c2`:

```dts
gt9147: touchscreen@14 {
    compatible = "goodix,gt9147", "goodix,gt9xx";
    reg = <0x14>;

    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_tsc &pinctrl_tsc_reset>;

    interrupt-parent = <&gpio1>;
    interrupts = <9 0>;
    goodix,rst-gpio = <&gpio5 9 GPIO_ACTIVE_LOW>;
    goodix,irq-gpio = <&gpio1 9 GPIO_ACTIVE_LOW>;

    status = "okay";
};
```

For GT9147, configure the interrupt GPIO as follows in `&iomuxc`:

```dts
pinctrl_tsc: tscgrp {
    fsl,pins = <
        MX6UL_PAD_GPIO1_IO09__GPIO1_IO09 0x10B0
    >;
};
```

Configure the reset GPIO in `&iomuxc_snvs`:

```dts
pinctrl_tsc_reset: tsc_reset {
    fsl,pins = <
        MX6ULL_PAD_SNVS_TAMPER9__GPIO5_IO09 0x10B0
    >;
};
```

Disable alternate touchscreen nodes so they do not claim the same pins:

```dts
ft5426: touchscreen@38 {
    status = "disabled";
};

edt-ft5x06@38 {
    status = "disabled";
};

goodix_ts@5d {
    status = "disabled";
};

&tsc {
    status = "disabled";
};
```

There must be only one `pinctrl_tsc:` label in the DTS. The GT9147 setting is
`0x10B0`; the `0xF080` setting in some reference files is for FT5426 and
should not be used here.

## Kernel Driver

The kernel configuration must include the Goodix driver:

```text
CONFIG_TOUCHSCREEN_GT9XX=y
```

`=y` builds the driver into `zImage`, so no separate `.ko` file or `insmod`
command is required. The unrelated FT5x06 module should not be used: the
physical controller responds at `0x14`, while FT5x06 was configured for
`0x38`.

## Build and Deploy

From the kernel source root:

```sh
export PATH=/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin:$PATH
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j4 zImage imx6ull-alientek-emmc.dtb
```

Deploy the newly generated files to the boot method in use:

```text
arch/arm/boot/zImage
arch/arm/boot/dts/imx6ull-alientek-emmc.dtb
```

Reboot the board after updating the DTB. Saving the DTS file alone does not
change the device tree currently running on the board.

## Verify the Kernel Layer

After boot, verify that the Goodix driver bound to address `0x14`:

```sh
dmesg | grep -i goodix
```

Expected messages include:

```text
goodix-ts 1-0014: ID 911, version: 1060
input: goodix-ts as /devices/virtual/input/input1
```

Then find the input event node:

```sh
cat /proc/bus/input/devices
```

Expected form:

```text
N: Name="goodix-ts"
H: Handlers=event1
```

The event number is not guaranteed to remain `event1`; always use the value
reported by the current board.

To test raw input without Qt, stop the Qt program and run:

```sh
hexdump -C /dev/input/event1
```

Touching the screen should produce data. If no data appears, verify that the
board is booting the newly compiled DTB and recheck `GPIO1_IO09` with the
GT9147 pin configuration above.

## Qt Startup Script

Qt reads the kernel input event through its `evdevtouch` generic plugin. For
the current board, the startup script should be:

```sh
#!/bin/sh

export QTDIR=/usr/local/qt5.6.3
export LD_LIBRARY_PATH=$QTDIR/lib:$LD_LIBRARY_PATH
export QT_QPA_PLATFORM=linuxfb
export QT_QPA_PLATFORM_PLUGIN_PATH=$QTDIR/plugins/platforms
export QT_PLUGIN_PATH=$QTDIR/plugins
export QT_QPA_GENERIC_PLUGINS=evdevtouch:/dev/input/event1

exec /opt/light_panel/light_panel
```

Replace `event1` with the Goodix event node reported by
`/proc/bus/input/devices`.

For one-time plugin diagnostics, add this before `exec`:

```sh
export QT_DEBUG_PLUGINS=1
```

The log should show that Qt loaded:

```text
libqevdevtouchplugin.so
```

## Troubleshooting Order

1. `i2cdetect -r -y 1` must show `14`.
2. `dmesg | grep -i goodix` must show `goodix-ts 1-0014`.
3. `/proc/bus/input/devices` must show `goodix-ts` and an `eventX` node.
4. Reading `/dev/input/eventX` must produce data when the panel is touched.
5. Only then configure `QT_QPA_GENERIC_PLUGINS=evdevtouch:/dev/input/eventX`.

If step 4 fails, the issue is in the device tree pin configuration or the
kernel input path, not in Qt.
