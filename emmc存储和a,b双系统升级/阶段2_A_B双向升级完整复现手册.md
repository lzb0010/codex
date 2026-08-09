# 阶段2：i.MX6ULL eMMC A/B 双系统升级完整复现手册

适用环境：i.MX6ULL 正点原子 eMMC 板、U-Boot 2016.03、Buildroot/Linux 4.1.15。

本文是已经实际验证成功的第二阶段流程：A 升级到 B、B 升级回 A；两套系统均可启动，/data 自动挂载，静态网络、fw_setenv、mkfs.ext4 均可用。后续重新做时，按本文顺序操作即可复现。

## 1. 设计与最终分区

| 分区 | 用途 | 最终内容 |
|---|---|---|
| /dev/mmcblk1 原始区 | SPL、U-Boot、U-Boot 环境 | u-boot.imx、环境变量 |
| /dev/mmcblk1p1 | FAT 启动分区 | A/B 内核与设备树 |
| /dev/mmcblk1p2 | Rootfs A | A 系统根文件系统 |
| /dev/mmcblk1p3 | Rootfs B | B 系统根文件系统 |
| /dev/mmcblk1p4 | Data | /data、升级包、日志、状态 |

p1 中必须使用下面固定文件名，Linux 文件名区分大小写：

    zimage_a
    imx6ull-alientek-emmc_a.dtb
    zimage_b
    imx6ull-alientek-emmc_b.dtb

升级包固定保存位置：

    /data/update       A -> B 的 B 升级包
    /data/update_a     B -> A 的 A 升级包

## 2. eMMC U-Boot 与 A/B 启动环境

### 2.1 首次写入 eMMC U-Boot

在 U-Boot 中执行：

    tftpboot \${loadaddr} u-boot.imx
    mmc dev 1
    mmc write \${loadaddr} 0x2 0x346

命令作用：

- tftpboot：把 Ubuntu TFTP 目录中的 u-boot.imx 下载到 DDR。
- mmc dev 1：选择 eMMC；本项目中 mmc1 是 eMMC。
- mmc write：从原始 LBA 2 开始写入 0x346 个 512 字节块。

功能：将 SPL 和 U-Boot 写入 eMMC 原始启动区。

目的：拔掉 SD 卡后，芯片 ROM 能从 eMMC 启动 U-Boot。

成功标准：拔掉 SD 卡重新上电，串口出现 U-Boot 提示符。

可选校验：

    mmc read 0x81000000 0x2 0x346
    cmp.b \${loadaddr} 0x81000000 0x68c00

说明：u-boot.imx 大小为 0x68c00 字节，向上取整为 0x346 个扇区。U-Boot 后面的原始数据不是文件拼接区；启动 ROM 根据镜像本身加载，不需要保留旧镜像后面的残留字节。

### 2.2 配置 U-Boot A/B 启动

在 eMMC U-Boot 中一次性执行：

    setenv ethaddr b8:ae:1d:01:00:00
    setenv boot_slot A
    setenv mmcpart 1
    setenv boot_fdt yes
    setenv loadimage 'fatload mmc \${mmcdev}:\${mmcpart} \${loadaddr} \${image}'
    setenv loadfdt 'fatload mmc \${mmcdev}:\${mmcpart} \${fdt_addr} \${fdt_file}'
    setenv mmcargs 'setenv bootargs console=\${console},\${baudrate} root=\${mmcroot}'
    setenv mmcboot 'echo Booting from mmc ...; run mmcargs; if run loadfdt; then bootz \${loadaddr} - \${fdt_addr}; else echo WARN: Cannot load the DT; fi'
    setenv boot_a 'setenv mmcdev 1; setenv mmcroot /dev/mmcblk1p2 rootwait rw; setenv image zimage_a; setenv fdt_file imx6ull-alientek-emmc_a.dtb; run loadimage; run mmcboot'
    setenv boot_b 'setenv mmcdev 1; setenv mmcroot /dev/mmcblk1p3 rootwait rw; setenv image zimage_b; setenv fdt_file imx6ull-alientek-emmc_b.dtb; run loadimage; run mmcboot'
    setenv bootcmd 'if test \${boot_slot} = A; then run boot_a; else run boot_b; fi'
    saveenv
    printenv ethaddr boot_slot bootcmd

命令作用：

- boot_a：指定 p2 为根分区，并加载 A 的 zimage_a 和 dtb。
- boot_b：指定 p3 为根分区，并加载 B 的 zimage_b 和 dtb。
- bootcmd：依据 boot_slot 选择 boot_a 或 boot_b。
- saveenv：将上述环境变量写入 eMMC 的 U-Boot 环境区。

功能：用一个 boot_slot 变量切换整套启动组合。

目的：运行 A 时只改写 B；运行 B 时只改写 A，永远不直接格式化当前正在运行的根文件系统。

成功标准：

    reset

A 槽启动后：

    cat /proc/cmdline

输出应包含 root=/dev/mmcblk1p2。B 槽则应包含 root=/dev/mmcblk1p3。

相关知识：本阶段是手动切槽的 A/B 升级。Bootcount 自动回滚属于下一阶段增强功能；它用于统计新槽连续启动失败次数，不能代替升级包校验和安全写入。

## 3. Linux 中读写 U-Boot 环境

确认 U-Boot 源码配置路径：

    /home/huanyu/linux/uboot/uboot-imx-rel_imx_4.1.15_2.1.0_ga_alientek/include/configs/mx6ull_alientek_emmc.h

已确认的环境配置：

    CONFIG_ENV_IS_IN_MMC
    CONFIG_SYS_MMC_ENV_DEV = 1
    CONFIG_ENV_OFFSET = 12 * 64 KiB = 0xC0000
    CONFIG_ENV_SIZE = 8 KiB = 0x2000

在 Buildroot 中启用：

    BR2_PACKAGE_UBOOT_TOOLS=y
    BR2_PACKAGE_UBOOT_TOOLS_FWPRINTENV=y

在目标根文件系统创建 /etc/fw_env.config：

    /dev/mmcblk1  0xC0000  0x2000

验证：

    fw_printenv boot_slot

命令作用：fw_printenv 读取 U-Boot 环境，fw_setenv 写入 U-Boot 环境。

功能：Linux 升级脚本可在完成全部文件写入后执行 fw_setenv boot_slot A 或 B。

目的：保证只有升级真正完成后，下次重启才会进入新槽。

成功标准：fw_printenv boot_slot 输出 boot_slot=A 或 boot_slot=B。

## 4. p4 自动挂载与网络

目标 rootfs 的 /etc/fstab 要有：

    /dev/mmcblk1p4  /data  ext4  defaults  0  2

目标 rootfs 的 /etc/init.d/S99static_network 内容：

    #!/bin/sh
    # Buildroot 的 rcS 不会自动遍历 S99*，所以必须由 rcS 显式调用。
    case "$1" in
    start)
        ifconfig eth0 192.168.3.50 netmask 255.255.255.0 up
        route del default 2>/dev/null
        route add default gw 192.168.3.1
        ;;
    esac

并在 /etc/init.d/rcS 最后加入：

    /etc/init.d/S99static_network start

命令作用：fstab 在开机阶段挂载 p4；网络脚本配置 eth0 地址、掩码、默认网关。

功能：所有槽启动后都能自动得到 /data 和网络。

目的：升级包存放在 /data；没有默认网关时会出现 Network is unreachable，无法进行跨网段访问。

成功标准：

    mount | grep mmcblk1p4
    ping 192.168.3.66

第一条显示 /dev/mmcblk1p4 on /data，第二条能够 ping 通 Ubuntu。

## 5. Ubuntu 制作升级包

Ubuntu IP：192.168.3.66。TFTP 根目录：/home/huanyu/linux/tftp。

### 5.1 制作 B 包，供 A -> B 使用

目录约定：

    根文件来源：/home/huanyu/linux/rootfs_nogpu
    升级包目录：/home/huanyu/linux/update
    已放入包目录：zImage_b、imx6ull-alientek-emmc_b.dtb

创建 /home/huanyu/linux/update/make_rootfs_b.sh：

    #!/bin/bash
    set -e
    
    ROOTFS_SRC=/home/huanyu/linux/rootfs_nogpu
    OUT=/home/huanyu/linux/update
    mkdir -p "$OUT"
    
    # dev/proc/sys 等属于运行期挂载点，不能把当前挂载内容打入镜像。
    sudo tar --numeric-owner -czpf "$OUT/rootfs_b.tar.gz" \
      --exclude='./dev/*' --exclude='./proc/*' --exclude='./sys/*' \
      --exclude='./tmp/*' --exclude='./run/*' --exclude='./mnt/*' \
      -C "$ROOTFS_SRC" .
    
    cd "$OUT"
    sha256sum rootfs_b.tar.gz zImage_b imx6ull-alientek-emmc_b.dtb > sha256sum_b.txt

执行：

    chmod +x /home/huanyu/linux/update/make_rootfs_b.sh
    /home/huanyu/linux/update/make_rootfs_b.sh
    cp /home/huanyu/linux/update/rootfs_b.tar.gz \
       /home/huanyu/linux/update/zImage_b \
       /home/huanyu/linux/update/imx6ull-alientek-emmc_b.dtb \
       /home/huanyu/linux/update/sha256sum_b.txt \
       /home/huanyu/linux/tftp/

### 5.2 制作 A 包，供 B -> A 使用

目录约定：

    根文件来源：/home/huanyu/linux/rootfs_gst
    升级包目录：/home/huanyu/linux/update_a
    已放入包目录：zImage_a、imx6ull-alientek-emmc_a.dtb

创建 /home/huanyu/linux/update_a/make_rootfs_a.sh：

    #!/bin/bash
    set -e
    
    ROOTFS_SRC=/home/huanyu/linux/rootfs_gst
    OUT=/home/huanyu/linux/update_a
    mkdir -p "$OUT"
    
    sudo tar --numeric-owner -czpf "$OUT/rootfs_a.tar.gz" \
      --exclude='./dev/*' --exclude='./proc/*' --exclude='./sys/*' \
      --exclude='./tmp/*' --exclude='./run/*' --exclude='./mnt/*' \
      -C "$ROOTFS_SRC" .
    
    cd "$OUT"
    sha256sum rootfs_a.tar.gz zImage_a imx6ull-alientek-emmc_a.dtb > sha256sum_a.txt

执行后将 rootfs_a.tar.gz、zImage_a、imx6ull-alientek-emmc_a.dtb、sha256sum_a.txt 复制到 TFTP 根目录。

功能：将根文件目录、内核、dtb 和校验值组合为一套可验证的升级包。

目的：根文件系统必须先格式化目标 ext4 分区，再解压到该分区；不能把目录当作一个普通文件直接写入。

成功标准：每个包目录中都有 rootfs_X.tar.gz、zImage_X、对应 dtb、sha256sum_X.txt。

## 6. 下载升级包到 p4

以当前运行 A、下载 B 包为例：

    mkdir -p /data/update
    cd /data/update
    tftp -g -r rootfs_b.tar.gz -l rootfs_b.tar.gz.part 192.168.3.66 && mv rootfs_b.tar.gz.part rootfs_b.tar.gz
    tftp -g -r zImage_b -l zImage_b.part 192.168.3.66 && mv zImage_b.part zImage_b
    tftp -g -r imx6ull-alientek-emmc_b.dtb -l imx6ull-alientek-emmc_b.dtb.part 192.168.3.66 && mv imx6ull-alientek-emmc_b.dtb.part imx6ull-alientek-emmc_b.dtb
    tftp -g -r sha256sum_b.txt -l sha256sum_b.txt.part 192.168.3.66 && mv sha256sum_b.txt.part sha256sum_b.txt
    sha256sum -c sha256sum_b.txt

功能：从 Ubuntu 下载完整升级包到独立的数据分区。

目的：使用 .part 临时文件，网络中断不会留下被误认为有效包的半截文件。

成功标准：三项 sha256 校验均输出 OK。

B -> A 时目录改为 /data/update_a，文件名改为 A 包的四个文件。

## 7. 最终通用升级脚本

把下列脚本保存为 /data/update/upgrade_ab.sh，再执行 chmod +x /data/update/upgrade_ab.sh。

    #!/bin/sh
    # upgrade_ab.sh
    # 自动识别当前槽，只格式化未运行的槽。
    # 校验、解压、写 p1 都成功后，才切换 boot_slot。
    set -eu
    
    ROOT_MNT=/mnt/ab_target_rootfs
    BOOT_MNT=/mnt/ab_boot_p1
    
    fail()
    {
        echo "ERROR: $*" >&2
        exit 1
    }
    
    cleanup()
    {
        umount "$ROOT_MNT" 2>/dev/null || true
        umount "$BOOT_MNT" 2>/dev/null || true
    }
    trap cleanup EXIT INT TERM
    
    CURRENT_ROOT="$(sed -n 's/.*root=\\([^ ]*\\).*/\\1/p' /proc/cmdline)"
    case "$CURRENT_ROOT" in
        /dev/mmcblk1p2)
            CURRENT_SLOT=A
            TARGET_SLOT=B
            TARGET_ROOT=/dev/mmcblk1p3
            TARGET_LABEL=rootfs_b
            PKG_DIR=/data/update
            ROOTFS=rootfs_b.tar.gz
            SHA_FILE=sha256sum_b.txt
            KERNEL_SRC=zImage_b
            KERNEL_DST=zimage_b
            DTB=imx6ull-alientek-emmc_b.dtb
            ;;
        /dev/mmcblk1p3)
            CURRENT_SLOT=B
            TARGET_SLOT=A
            TARGET_ROOT=/dev/mmcblk1p2
            TARGET_LABEL=rootfs_a
            PKG_DIR=/data/update_a
            ROOTFS=rootfs_a.tar.gz
            SHA_FILE=sha256sum_a.txt
            KERNEL_SRC=zImage_a
            KERNEL_DST=zimage_a
            DTB=imx6ull-alientek-emmc_a.dtb
            ;;
        *) fail "未知当前根分区：$CURRENT_ROOT" ;;
    esac
    
    echo "当前槽：$CURRENT_SLOT；目标槽：$TARGET_SLOT"
    
    # 防呆：U-Boot 环境必须和当前实际根分区一致。
    fw_printenv boot_slot | grep -qx "boot_slot=$CURRENT_SLOT" ||
        fail "boot_slot 与当前根分区不一致"
    
    cd "$PKG_DIR" || fail "找不到升级目录：$PKG_DIR"
    [ -f "$ROOTFS" ] && [ -f "$SHA_FILE" ] &&
    [ -f "$KERNEL_SRC" ] && [ -f "$DTB" ] ||
        fail "升级包不完整"
    
    # 格式化前必须先同时通过 SHA256 和 gzip 数据完整性校验。
    sha256sum -c "$SHA_FILE"
    gzip -t "$ROOTFS"
    
    # 禁止格式化已经挂载的目标分区。
    grep -q "^$TARGET_ROOT " /proc/mounts &&
        fail "$TARGET_ROOT 已挂载，拒绝格式化"
    
    # 下一槽需要有 mkfs.ext4，才能完成以后反向升级。
    [ -x /sbin/mke2fs.ext4 ] || fail "缺少 /sbin/mke2fs.ext4"
    mkfs.ext4 -V >/dev/null
    
    mkdir -p "$ROOT_MNT" "$BOOT_MNT"
    echo "格式化 $TARGET_ROOT ..."
    mkfs.ext4 -F -L "$TARGET_LABEL" "$TARGET_ROOT"
    mount "$TARGET_ROOT" "$ROOT_MNT"
    
    echo "解压根文件系统 ..."
    # BusyBox tar 不支持 tar -xzf，必须先由 gzip 解压。
    gzip -dc "$ROOTFS" | tar -x -f - -C "$ROOT_MNT"
    
    # 重新创建打包时排除的运行期目录。
    mkdir -p "$ROOT_MNT/dev" "$ROOT_MNT/proc" "$ROOT_MNT/sys" \
             "$ROOT_MNT/tmp" "$ROOT_MNT/run" "$ROOT_MNT/mnt"
    chmod 1777 "$ROOT_MNT/tmp"
    
    # 新槽启动后自动挂载 p4。
    mkdir -p "$ROOT_MNT/data"
    [ -f "$ROOT_MNT/etc/fstab" ] || : > "$ROOT_MNT/etc/fstab"
    grep -q '^/dev/mmcblk1p4[[:space:]]' "$ROOT_MNT/etc/fstab" ||
        printf '/dev/mmcblk1p4  /data  ext4  defaults  0  2\\n' >> "$ROOT_MNT/etc/fstab"
    
    # 新槽启动后自动配置静态网络。
    mkdir -p "$ROOT_MNT/etc/init.d"
    cat > "$ROOT_MNT/etc/init.d/S99static_network" <<'NETEOF'
    #!/bin/sh
    case "$1" in
    start)
        ifconfig eth0 192.168.3.50 netmask 255.255.255.0 up
        route del default 2>/dev/null
        route add default gw 192.168.3.1
        ;;
    esac
    NETEOF
    chmod 755 "$ROOT_MNT/etc/init.d/S99static_network"
    [ -f "$ROOT_MNT/etc/init.d/rcS" ] || fail "目标槽没有 rcS"
    grep -qx '/etc/init.d/S99static_network start' "$ROOT_MNT/etc/init.d/rcS" ||
        printf '\\n/etc/init.d/S99static_network start\\n' >> "$ROOT_MNT/etc/init.d/rcS"
    
    # 复制 fw_setenv 和 mkfs.ext4，使新槽可再次反向升级。
    mkdir -p "$ROOT_MNT/usr/sbin" "$ROOT_MNT/sbin" "$ROOT_MNT/usr/lib" "$ROOT_MNT/lib"
    cp -a /usr/sbin/fw_printenv "$ROOT_MNT/usr/sbin/"
    ln -sf fw_printenv "$ROOT_MNT/usr/sbin/fw_setenv"
    cp -a /etc/fw_env.config "$ROOT_MNT/etc/"
    cp -a /sbin/mke2fs.ext4 "$ROOT_MNT/sbin/"
    ln -sf mke2fs.ext4 "$ROOT_MNT/sbin/mkfs.ext4"
    for lib in \
        /usr/lib/libext2fs.so.2* /usr/lib/libcom_err.so.2* /usr/lib/libe2p.so.2* \
        /lib/libblkid.so.1* /lib/libuuid.so.1*
    do
        [ -e "$lib" ] && cp -a "$lib" "$ROOT_MNT$(dirname "$lib")/"
    done
    
    sync
    umount "$ROOT_MNT"
    
    # 最后更新 p1 中目标槽对应的内核和设备树。
    mount -t vfat /dev/mmcblk1p1 "$BOOT_MNT"
    cp -f "$KERNEL_SRC" "$BOOT_MNT/$KERNEL_DST"
    cp -f "$DTB" "$BOOT_MNT/$DTB"
    sync
    umount "$BOOT_MNT"
    
    # 所有写入完成后才切槽。
    fw_setenv boot_slot "$TARGET_SLOT"
    fw_printenv boot_slot | grep -qx "boot_slot=$TARGET_SLOT" ||
        fail "boot_slot 写入校验失败"
    echo "升级完成：重启后进入 $TARGET_SLOT 槽。"

脚本功能：校验升级包，格式化未运行槽，解压 rootfs，安装网络和数据盘配置，保留反向升级工具，更新目标槽的内核和 dtb，最后切换 boot_slot。

脚本目的：任一时刻至少保持当前槽完整可启动；若校验、格式化或解压失败，脚本不会切换 boot_slot。

特别说明：

- 不可使用 tar -xzf。当前 BusyBox tar 不支持 z 参数。
- 必须使用 gzip -dc 文件 | tar -x -f -。
- 必须把 mke2fs.ext4 及 libext2fs、libcom_err、libe2p、libblkid、libuuid 一起带到新槽，否则下一次反向升级会失败。
- Buildroot 的 rcS 不会自动执行 S99static_network，必须写入 rcS 的显式调用。

## 8. 执行升级与验证

执行：

    chmod +x /data/update/upgrade_ab.sh
    /data/update/upgrade_ab.sh

命令作用：自动判断当前是 A 还是 B，并升级另一槽。

成功标准：最后显示“升级完成：重启后进入 X 槽”，随后：

    sync
    reboot

新系统启动后验证：

    cat /proc/cmdline
    mount | grep ' on / '
    mount | grep mmcblk1p4
    fw_printenv boot_slot
    ping 192.168.3.66
    mkfs.ext4 -V

预期结果：

| 当前槽 | /proc/cmdline 根分区 | boot_slot |
|---|---|---|
| A | /dev/mmcblk1p2 | A |
| B | /dev/mmcblk1p3 | B |

此外必须确认 /dev/mmcblk1p4 已挂载到 /data，ping 能通 Ubuntu，mkfs.ext4 -V 能输出 mke2fs 版本。

## 9. 故障对照

### tar: invalid option -- z

原因：BusyBox tar 不支持 -z。

处理：

    gzip -t rootfs_X.tar.gz
    gzip -dc rootfs_X.tar.gz | tar -x -f - -C /mnt/目标目录

### tar: invalid tar magic

原因：gzip 压缩流直接给了 tar。

处理：必须先用 gzip -dc 解压，再让 tar 从标准输入读取；并先运行 gzip -t。

### mkfs.ext4: not found 或缺少 libext2fs.so.2

原因：新槽没有复制 mke2fs.ext4 或运行库。

处理：使用第 7 节最终脚本；其中已经复制以下内容：

    /sbin/mke2fs.ext4
    /sbin/mkfs.ext4 -> mke2fs.ext4
    /usr/lib/libext2fs.so.2*
    /usr/lib/libcom_err.so.2*
    /usr/lib/libe2p.so.2*
    /lib/libblkid.so.1*
    /lib/libuuid.so.1*

验证：

    mkfs.ext4 -V

### 网络提示 Network is unreachable

原因：默认网关没有配置，或者 rcS 没有调用网络脚本。

处理：检查 S99static_network 的 IP/网关，并检查 rcS 最后一行存在：

    /etc/init.d/S99static_network start

## 10. 最终安全原则

1. 永远不格式化当前根分区。
2. SHA256 和 gzip 校验不通过时绝不开始格式化。
3. boot_slot 必须是最后一个写入操作。
4. 新槽必须自带 /data 自动挂载、网络、fw_setenv、mkfs.ext4 和依赖库。
5. 重启并完成根分区、p4、网络、环境变量、mkfs.ext4 五项验证后，才认为升级成功。

以上流程已经完成 A -> B 与 B -> A 的实机验证，可作为第二阶段结束版本和后续 Bootcount 自动回滚阶段的基础。
