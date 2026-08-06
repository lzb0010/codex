# 阶段 1：eMMC 存储布局、ext4 工具与文件系统准备

> 项目：基于 i.MX6ULL 的 eMMC 可靠存储、A/B 安全升级与故障自恢复系统  
> 状态：已完成并验证  
> 开发板：正点原子 i.MX6ULL

## 1. 阶段目标

完成板载 eMMC 的识别、重新分区、ext4 工具部署、文件系统初始化和只读挂载验证，为后续 A/B 根文件系统、升级包、日志和状态数据提供可靠的独立存储空间。

本阶段的关键前提是：开发板当前由 **TFTP 加载内核/DTB，NFS 挂载根文件系统** 启动。因此操作 eMMC 时，运行中的根文件系统不在 eMMC 上，不会因格式化 `mmcblk1p2/p3/p4` 而中断当前开发环境。

## 2. 当前启动环境确认

开发板内核命令行：

```text
console=ttymxc0,115200 root=/dev/nfs rw \
nfsroot=192.168.3.66:/home/huanyu/linux/rootfs_gst,v3,tcp \
ip=192.168.3.50:192.168.3.66:192.168.3.1:255.255.255.0::eth0:off
```

| 项目 | 当前配置 | 作用 |
| --- | --- | --- |
| 内核和 DTB | TFTP 加载 | 便于频繁调试内核和设备树 |
| 根文件系统 | NFS：`192.168.3.66:/home/huanyu/linux/rootfs_gst` | 当前系统不依赖 eMMC 根分区 |
| 开发板 IP | `192.168.3.50` | 开发板网络地址 |
| Ubuntu 主机 IP | `192.168.3.66` | TFTP/NFS 服务端地址 |

用于确认的命令：

```sh
cat /proc/cmdline
mount | head -n 20
```

预期看到 `/` 挂载为 `192.168.3.66:/home/huanyu/linux/rootfs_gst on / type nfs`。

## 3. 存储设备识别

在开发板执行：

```sh
cat /proc/partitions

for dev in /sys/block/mmcblk*; do
    echo "===== $dev ====="
    cat "$dev/device/type"
    cat "$dev/device/name"
    cat "$dev/device/cid"
done

dmesg | grep -i -E "mmc|usdhc|sdhci"
```

确认结论：

| 设备 | 类型 | 容量/型号 | 处理原则 |
| --- | --- | --- | --- |
| `/dev/mmcblk0` | SD 卡 | 58.2 GiB | 不参与本项目 eMMC 分区操作 |
| `/dev/mmcblk1` | eMMC | `8GTF4R`，约 7.28 GiB | 本项目目标设备 |
| `/dev/mmcblk1boot0`、`boot1` | eMMC Boot Area | 各 4 MiB | 本阶段禁止操作 |
| `/dev/mmcblk1rpmb` | RPMB | 512 KiB | 本阶段禁止操作 |

日志中的驱动链路：

```text
i.MX6ULL USDHC2 控制器
    → sdhci-esdhc-imx 驱动
    → MMC 子系统
    → mmcblk1 块设备
    → ext4 文件系统
```

对应日志：

```text
mmc1: SDHCI controller on 2194000.usdhc using ADMA
mmc1: new DDR MMC card at address 0001
mmcblk1: mmc1:0001 8GTF4R 7.28 GiB
```

## 4. 在 Buildroot 中加入完整 ext4 工具

### 4.1 为什么需要额外加入 e2fsprogs

原根文件系统中的 BusyBox `mke2fs` 是精简实现，不能使用完整 e2fsprogs 的参数。例如：

```sh
mke2fs -t ext4 -L rootfs_a /dev/mmcblk1p2
```

会报：

```text
mke2fs: invalid option -- 't'
```

因此需要从 Buildroot 编译并部署完整的 `mkfs.ext4` 和 `e2fsck`。部署后，保留 BusyBox 的 `/sbin/mke2fs` 不动，使用新命令 `/sbin/mkfs.ext4`。

### 4.2 进入 Buildroot 配置

在 Ubuntu 执行：

```sh
cd /home/huanyu/linux/buildroot-2022.02.10
make menuconfig
```

如果 `make menuconfig` 报错：

```text
error while loading shared libraries: libncurses.so.5
```

先在 Ubuntu 安装兼容开发库，再重新执行菜单配置：

```sh
sudo apt update
sudo apt install libncurses5-dev
```

在菜单中进入：

```text
Target packages
  → Filesystem and flash utilities
    → e2fsprogs
```

勾选：

```text
[*] e2fsprogs
[*] e2fsprogs fsck
```

保存退出后，配置中应出现：

```sh
grep "BR2_PACKAGE_E2FSPROGS" .config
```

预期至少包含：

```text
BR2_PACKAGE_E2FSPROGS=y
BR2_PACKAGE_E2FSPROGS_FSCK=y
```

### 4.3 编译并确认生成 ARM 工具

```sh
cd /home/huanyu/linux/buildroot-2022.02.10
make -j"$(nproc)"

find output/target \( -name mkfs.ext4 -o -name mke2fs -o -name e2fsck \) -ls
file -L output/target/sbin/mkfs.ext4

readelf -d output/target/sbin/mke2fs | grep NEEDED
readelf -d output/target/sbin/e2fsck | grep NEEDED
```

实际生成结果：

```text
output/target/sbin/mke2fs
output/target/sbin/mkfs.ext4 -> mke2fs
output/target/sbin/e2fsck
```

`file` 的关键结果应为：

```text
ELF 32-bit LSB shared object, ARM, EABI5, dynamically linked
```

### 4.4 将工具和依赖库部署到 NFS 根文件系统

在 Ubuntu 新建文件 `deploy_e2fsprogs_to_nfs_rootfs.sh`，内容如下：

```bash
#!/usr/bin/env bash
set -euo pipefail

BUILDROOT_DIR="${BUILDROOT_DIR:-/home/huanyu/linux/buildroot-2022.02.10}"
NFS_ROOT="${NFS_ROOT:-/home/huanyu/linux/rootfs_gst}"
TARGET_DIR="$BUILDROOT_DIR/output/target"

die() {
    echo "ERROR: $*" >&2
    exit 1
}

require_file() {
    [ -e "$1" ] || die "Required file is missing: $1"
}

require_dir() {
    [ -d "$1" ] || die "Required directory is missing: $1"
}

require_dir "$BUILDROOT_DIR"
require_dir "$TARGET_DIR"
require_dir "$NFS_ROOT"
require_file "$TARGET_DIR/sbin/mke2fs"
require_file "$TARGET_DIR/sbin/e2fsck"

file -L "$TARGET_DIR/sbin/mke2fs" | grep -q "ARM" \
    || die "mke2fs is not an ARM executable"

install -d "$NFS_ROOT/sbin" "$NFS_ROOT/lib" "$NFS_ROOT/usr/lib" "$NFS_ROOT/etc" "$NFS_ROOT/opt"

cp -a "$TARGET_DIR"/usr/lib/libext2fs.so.2* "$NFS_ROOT/usr/lib/"
cp -a "$TARGET_DIR"/usr/lib/libcom_err.so.2* "$NFS_ROOT/usr/lib/"
cp -a "$TARGET_DIR"/usr/lib/libe2p.so.2* "$NFS_ROOT/usr/lib/"
cp -a "$TARGET_DIR"/lib/libblkid.so.1* "$NFS_ROOT/lib/"
cp -a "$TARGET_DIR"/lib/libuuid.so.1* "$NFS_ROOT/lib/"

# 不替换 BusyBox mke2fs，避免影响已有工具行为。
install -m 0755 "$TARGET_DIR/sbin/mke2fs" "$NFS_ROOT/sbin/mke2fs.ext4"
install -m 0755 "$TARGET_DIR/sbin/e2fsck" "$NFS_ROOT/sbin/e2fsck.ext4"
ln -sfn mke2fs.ext4 "$NFS_ROOT/sbin/mkfs.ext4"
ln -sfn e2fsck.ext4 "$NFS_ROOT/sbin/fsck.ext4"

if [ -f "$TARGET_DIR/etc/mke2fs.conf" ]; then
    cp -a "$TARGET_DIR/etc/mke2fs.conf" "$NFS_ROOT/etc/"
fi

echo "e2fsprogs deployment completed"
echo "Use on the board: mkfs.ext4 -n /dev/mmcblk1p2"
```

执行：

```sh
chmod +x deploy_e2fsprogs_to_nfs_rootfs.sh
./deploy_e2fsprogs_to_nfs_rootfs.sh
```

由于根文件系统通过 NFS 挂载，部署完成后无需重新制作根文件系统镜像或重启开发板。

在开发板验证：

```sh
mkfs.ext4 -n /dev/mmcblk1p2
```

`-n` 为演练模式，只显示将如何创建 ext4 文件系统，**不会格式化分区**。成功时可以看到：

```text
mke2fs 1.46.5 (30-Dec-2021)
Creating filesystem with ...
```

> 后续所有格式化操作都使用 `mkfs.ext4`，不要使用 BusyBox 的 `mke2fs -t ext4`。

## 5. eMMC 分区操作

### 5.1 分区前检查

```sh
fdisk -l /dev/mmcblk1
```

执行分区操作前已明确确认：内核、设备树和根文件系统均保留在 Ubuntu 开发环境中，因此允许清空并重建 eMMC 的普通用户区分区表。

### 5.2 最终分区表

eMMC：`/dev/mmcblk1`，共 `15,269,888` 个扇区，每扇区 `512 B`。

| 分区 | 起始 LBA | 结束 LBA | 大小 | 用途 |
| --- | ---: | ---: | ---: | --- |
| `/dev/mmcblk1p1` | 20,480 | 282,623 | 128 MiB | FAT32 Boot；后续保存 A/B 内核与 DTB |
| `/dev/mmcblk1p2` | 282,624 | 2,379,775 | 1 GiB | Rootfs A |
| `/dev/mmcblk1p3` | 2,379,776 | 4,476,927 | 1 GiB | Rootfs B |
| `/dev/mmcblk1p4` | 4,476,928 | 15,269,887 | 约 5.27 GiB | Data |

逻辑关系：

```text
Boot(p1): zImage_A/dtb_A 与 zImage_B/dtb_B
Rootfs A(p2): 当前稳定系统
Rootfs B(p3): 新版本升级写入的非运行槽位
Data(p4): 升级包、升级状态、日志、用户数据、测试数据
```

### 5.3 fdisk 交互输入记录

进入：

```sh
fdisk /dev/mmcblk1
```

在 `fdisk` 中依次输入。下面的扇区值是本板 eMMC 的实际值，不能直接照搬到容量不同的设备。

```text
d                 # 删除原 p2
2
d                 # 删除原 p1
1

n                 # 建 p1：Boot
p
1
20480
282623
t                 # p1 类型设为 FAT32 LBA
1
c

n                 # 建 p2：Rootfs A
p
2
282624
2379775

n                 # 建 p3：Rootfs B
p
3
2379776
4476927

n                 # 建 p4：Data
p
4
4476928
15269887

p                 # 再次显示并核对分区表
w                 # 确认写入分区表
```

写入后检查：

```sh
sync
fdisk -l /dev/mmcblk1
```

> 分区相邻时，后一分区的起始扇区必须等于前一分区结束扇区加 1。例如 p3 的结束是 `4476927`，所以 p4 的起始必须为 `4476928`。否则 `fdisk` 会报扇区已被分配或范围非法。

## 6. 创建 ext4 文件系统

在开发板执行：

```sh
mkfs.ext4 -F -L rootfs_a /dev/mmcblk1p2
mkfs.ext4 -F -L rootfs_b /dev/mmcblk1p3
mkfs.ext4 -F -L data     /dev/mmcblk1p4
sync
```

参数说明：

| 参数 | 作用 |
| --- | --- |
| `mkfs.ext4` | 完整 e2fsprogs 提供的 ext4 格式化程序 |
| `-F` | 允许对块设备强制创建文件系统 |
| `-L` | 写入文件系统卷标，方便后续按标签识别分区 |
| `sync` | 将文件系统元数据和缓存写入 eMMC |

成功标志是每个分区均出现：

```text
Creating journal ... done
Writing superblocks and filesystem accounting information: done
```

## 7. 只读挂载验证

创建临时挂载目录并验证：

```sh
mkdir -p /tmp/rootfs_a_check
mkdir -p /tmp/rootfs_b_check
mkdir -p /tmp/data_check

mount -o ro,noload /dev/mmcblk1p2 /tmp/rootfs_a_check
mount -o ro,noload /dev/mmcblk1p3 /tmp/rootfs_b_check
mount -o ro,noload /dev/mmcblk1p4 /tmp/data_check

mount | grep mmcblk1

umount /tmp/rootfs_a_check
umount /tmp/rootfs_b_check
umount /tmp/data_check
```

`ro,noload` 的意义：

- `ro`：只读挂载，避免验证过程改写文件系统；
- `noload`：禁止 ext4 回放日志，避免“只读挂载”仍因为日志恢复而临时写入。

若日志中出现：

```text
EXT3-fs: unsupported optional features
EXT4-fs: mounted filesystem without journal. Opts: noload
```

这是正常现象。内核会先尝试 ext3，再由 ext4 驱动成功挂载；第二行就是成功依据。

## 8. 阶段验收结果

- [x] 当前根文件系统已确认来自 NFS，而不是 eMMC。
- [x] 已区分 SD 卡 `mmcblk0` 与板载 eMMC `mmcblk1`。
- [x] 未操作 eMMC 的 `boot0`、`boot1`、`rpmb` 特殊区域。
- [x] 已在 Buildroot 中启用 e2fsprogs 并编译出 ARM 版本 ext4 工具。
- [x] 已将 `mkfs.ext4`、`e2fsck.ext4` 和依赖库部署到 NFS 根文件系统。
- [x] 已完成 Boot / Rootfs A / Rootfs B / Data 四分区布局。
- [x] 已为 p2、p3、p4 创建带卷标的 ext4 文件系统。
- [x] 已以 `ro,noload` 方式验证三个 ext4 分区均可挂载。

## 9. 下一阶段

阶段 2 将把当前可运行的 NFS 根文件系统复制到 `Rootfs A (p2)`，建立第一套可启动 eMMC 根文件系统；再制作 Rootfs B，并逐步接入 U-Boot 槽位选择、启动计数、健康确认和自动回滚机制。

