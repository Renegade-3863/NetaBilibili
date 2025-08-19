#
#!/bin/bash
# 简易开发模式：如果修改了源码，那么自动进行重新编译 -> 重启服务器
set -e

# 可调整的 build 命令
build_cmd() 
{
    cd /app/build
    # 进入 /app 目录，在这个目录下调用编译命令
    # 也可以用更单纯一些的 gcc 命令，用这样的内容的话，就是直接调用 CMakeLists.txt 了，那么就需要保证存在对应的 CMakeLists.txt 文件
    cmake .. || return 1
    make
    return 0
}

# 启动服务器的函数，包装了一个运行命令行，以及设定了服务器进程的 PID
start_server() {
  # start the built server (use absolute path), write logs to /var/log
  /app/build/ReactorServer "8080" "/app/static/SimpleHttp/" > /var/log/reactor_server.log 2>&1 &
  server_pid=$!
  # give it a moment to start
  sleep 0.2
  echo "Server started pid=${server_pid}"
}

stop_server() {
    if [ -n "$server_pid" ] && kill -0 "$server_pid" 2>/dev/null; then 
        kill "$server_pid"
        wait "$server_pid" 2>/dev/null || true
        echo "Server stopped"
    fi
}

# 构建+启动
# 如果存在 build 文件夹，要先删除（使用 -rf 更安全）
rm -rf /app/build || true
mkdir -p /app/build
build_cmd || true
start_server

# 示例：更稳健的 rebuild+restart
# trap 命令，当 shell 接收到 SIGINT（INT）、SIGTERM（TERM）或脚本退出（EXIT）时，运行引号内的命令
trap 'echo "Stopping..."; stop_server; exit' INT TERM EXIT

rebuild_and_restart() {
  echo "Building..."
  # 如果重构成功，就终止旧进程，再启动新进程
  if build_cmd; then
    echo "Build succeeded."
    # 先停止旧进程，再启动新进程（或可改为先启动新进程再切换端口/负载）
    stop_server
    start_server
  # 否则，使用原进程
  else
    echo "Build failed; keeping old server running."
  fi
}

if command -v inotifywait >/dev/null 2>&1; then
  # 只监控 /app/src 并排除 /app/build
  # 使用 inotify 监听源码变更，但排除 build 目录和上传目录（如 /app/test），
  # 否则构建产物或上传大文件会触发重构循环。
  # 增加 debounce 到 1s，避免保存时的多次快速触发。
  while inotifywait -e modify,create,delete -r /app --exclude '/app/build|/app/build/.*|/app/test|/app/test/.*'; do 
    echo "Change detected, rebuilding..."
    # debounce: 等待收敛
    sleep 1
    rebuild_and_restart
  done
else
  # 常规轮询，每隔 2 秒检测一次更改
  while true; do
    echo "Is this working?"
    sleep 2
    rebuild_and_restart
  done
fi