#!/usr/bin/env bash
# 一键将 i.MX6ULL Qt 程序部署到 NFS 根文件系统

set -euo pipefail

APP_SOURCE="/home/huanyu/light_panel/build-imx6ull/light_panel"
TARGET_DIR="/home/huanyu/linux/rootfs_gst/opt/light_panel"

if [ ! -f "$APP_SOURCE" ]; then
    echo "未找到编译文件：$APP_SOURCE"
    echo "请先进入 /home/huanyu/light_panel/build-imx6ull 后执行 make -j4"
    exit 1
fi

mkdir -p "$TARGET_DIR"
install -m 755 "$APP_SOURCE" "$TARGET_DIR/light_panel"

echo "部署完成：$TARGET_DIR/light_panel"
echo "开发板运行：/opt/light_panel/start.sh"
