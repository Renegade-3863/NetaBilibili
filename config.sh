#!/usr/bin/env bash
# 一键临时/持久化 Linux 内核 & limits 调整，用于大并发压测（非生产默认）
# 以 root 执行：sudo bash superset_tune_1m.sh

set -euo pipefail

# ---------- 可调参数（按需修改） ----------
# 目标并发连接数估算（用于提醒）
TARGET_CONN=1000000

# fs.file-max（系统最大文件句柄）
FS_FILE_MAX=5000000

# 单进程打开句柄（建议至少比目标并发高20%）
PROCESS_NOFILE=1000000

# net.ipv4.ip_local_port_range (短期内扩大临时端口范围)
IP_LOCAL_PORT_LOW=1024
IP_LOCAL_PORT_HIGH=65535

# 其它内核 TCP/网络参数（常用组合）
SYSCTL_CONF=/etc/sysctl.d/99-neta-tuning.conf
LIMITS_CONF=/etc/security/limits.d/90-neta-nofile.conf
SYSTEMD_DROPIN_DIR=/etc/systemd/system/ReactorServer.service.d
SYSTEMD_DROPIN_FILE=$SYSTEMD_DROPIN_DIR/10-nofile.conf

# ---------- 立即生效（临时） ----------
echo "Applying immediate sysctl settings..."
sysctl -w fs.file-max=$FS_FILE_MAX
sysctl -w net.core.somaxconn=65535
sysctl -w net.core.netdev_max_backlog=300000
sysctl -w net.core.rmem_max=16777216
sysctl -w net.core.wmem_max=16777216
sysctl -w net.ipv4.tcp_max_syn_backlog=65535
sysctl -w net.ipv4.tcp_fin_timeout=30
sysctl -w net.ipv4.tcp_tw_reuse=1
# don't enable tcp_tw_recycle (unsafe); only use tw_reuse
sysctl -w net.ipv4.ip_local_port_range="${IP_LOCAL_PORT_LOW} ${IP_LOCAL_PORT_HIGH}"
sysctl -w net.ipv4.tcp_keepalive_time=300
sysctl -w net.ipv4.tcp_max_tw_buckets=500000
sysctl -w net.ipv4.tcp_mem="262144 524288 1048576"
sysctl -w vm.max_map_count=262144

echo "Setting shell limit for current session (root) ..."
ulimit -n $PROCESS_NOFILE 