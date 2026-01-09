#!/bin/bash
# 简单批量测试 MIHF 与所有 DLM 的交互（打印到当前终端）
# 用法: ./scripts/mihf_batch_test.sh
# 需要在仓库根目录运行（或将 MIHF_BIN 路径改为你的可执行路径）

set -euo pipefail
MIHF_BIN="/home/zhuwuhui/freeDiameter/MIHF_SIMULATOR/mihf_sim"
if [ ! -x "$MIHF_BIN" ]; then
  echo "找不到 MIHF 可执行: $MIHF_BIN" >&2
  exit 2
fi

# 启动 MIHF 作为 coprocess，这样它的 stdout/stderr 会直接显示到当前终端
IN_FIFO="/tmp/mihf_in.$$"
OUT_FIFO="/tmp/mihf_out.$$"
trap 'rm -f "$IN_FIFO" "$OUT_FIFO"' EXIT
mkfifo "$IN_FIFO" "$OUT_FIFO"

# 把 MIHF 的 stdout/stderr 打印到当前终端（后台）
cat "$OUT_FIFO" &
READER_PID=$!

# 启动 MIHF，stdin 从 IN_FIFO，stdout/stderr 写到 OUT_FIFO
"$MIHF_BIN" <"$IN_FIFO" >"$OUT_FIFO" 2>&1 &
MIHF_PID=$!

# 小工具：向 MIHF 发送命令并打印到终端
send_cmd() {
  local cmd="$1"
  printf "%s\n" "$cmd" > "$IN_FIFO"
  printf "# -> %s\n" "$cmd"
}

# 等待 MIHF 启动并监听（视机器速度可调整）
sleep 1

# 逐个 DLM (1=CELLULAR,2=SATCOM,3=WIFI) 发送一组原语
for idx in 1 2 3; do
  send_cmd "c $idx"   # Capability_Discover.request
  sleep 0.6
  send_cmd "p $idx"   # Get_Parameters.request
  sleep 0.6
  send_cmd "s $idx"   # Event_Subscribe.request
  sleep 0.6
  send_cmd "r $idx"   # Resource.request (简单示例)
  sleep 1.0
done

# 再发一次广播（可选）
send_cmd "a"
sleep 1.0

# 列出 DLM 套接字状态
send_cmd "l"
sleep 0.3

# 退出 MIHF
send_cmd "q"

# 等待 MIHF 退出，然后等待输出 reader 完成
wait ${MIHF_PID}
wait ${READER_PID}

exit 0
