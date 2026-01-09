# Makefile for MAGIC CM Core and DLM Daemons
# 编译 CM Core 服务器和三个 DLM 守护进程

CC = gcc
CFLAGS = -Wall -Wextra -pthread -I.
LDFLAGS = -pthread

# 目标文件
TARGETS = cm_core_server dlm_satcom_daemon dlm_cellular_daemon dlm_wifi_daemon

# 源文件目录
CM_DIR = magic_server/cm_core
DLM_SATCOM_DIR = DLM_SATCOM
DLM_CELLULAR_DIR = DLM_CELLULAR
DLM_WIFI_DIR = DLM_WIFI
LMI_DIR = magic_server/lmi

# 公共源文件
COMMON_SRCS = $(LMI_DIR)/magic_ipc_utils.c
COMMON_OBJS = magic_ipc_utils.o

.PHONY: all clean install

all: $(TARGETS)

# 编译公共模块
magic_ipc_utils.o: $(LMI_DIR)/magic_ipc_utils.c $(LMI_DIR)/magic_ipc_protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

# CM Core Server
cm_core_server: $(CM_DIR)/cm_core_server.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) $< $(COMMON_OBJS) $(LDFLAGS) -o $@
	@echo "✓ Built cm_core_server"

# DLM SATCOM Daemon
dlm_satcom_daemon: $(DLM_SATCOM_DIR)/dlm_satcom_daemon.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) $< $(COMMON_OBJS) $(LDFLAGS) -o $@
	@echo "✓ Built dlm_satcom_daemon"

# DLM CELLULAR Daemon
dlm_cellular_daemon: $(DLM_CELLULAR_DIR)/dlm_cellular_daemon.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) $< $(COMMON_OBJS) $(LDFLAGS) -o $@
	@echo "✓ Built dlm_cellular_daemon"

# DLM WIFI Daemon
dlm_wifi_daemon: $(DLM_WIFI_DIR)/dlm_wifi_daemon.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) $< $(COMMON_OBJS) $(LDFLAGS) -o $@
	@echo "✓ Built dlm_wifi_daemon"

# 安装到 /usr/local/bin
install: $(TARGETS)
	@echo "Installing binaries to /usr/local/bin..."
	install -m 0755 cm_core_server /usr/local/bin/
	install -m 0755 dlm_satcom_daemon /usr/local/bin/
	install -m 0755 dlm_cellular_daemon /usr/local/bin/
	install -m 0755 dlm_wifi_daemon /usr/local/bin/
	@echo "✓ Installation complete"

# 清理编译文件
clean:
	rm -f $(TARGETS) $(COMMON_OBJS)
	rm -f /tmp/magic_cm.sock
	@echo "✓ Cleaned"

# 帮助信息
help:
	@echo "MAGIC CM/DLM Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all     - Build all binaries"
	@echo "  clean   - Remove build artifacts"
	@echo "  install - Install to /usr/local/bin"
	@echo ""
	@echo "Binaries:"
	@echo "  cm_core_server        - Central Management Core server"
	@echo "  dlm_satcom_daemon     - SATCOM link monitor (eth1)"
	@echo "  dlm_cellular_daemon   - CELLULAR link monitor (eth2)"
	@echo "  dlm_wifi_daemon       - WIFI link monitor (eth3)"
