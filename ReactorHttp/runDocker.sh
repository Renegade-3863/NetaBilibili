#!/bin/bash

# 本机目录路径
HOST_DIR="/home/hzouaf/ReactorServer/SimpleHttp"

# 容器内挂载目录
CONTAINER_DIR="/app/test"

# 端口号
PORT=8080

# 运行容器，挂载目录并映射端口，传递参数给程序

# 下面是直接运行可执行程序的命令
docker run --rm -it -p ${PORT}:${PORT} -v \
    "${HOST_DIR}:${CONTAINER_DIR}" \
    reactorhttp-build \
    /app/build/ReactorServer \
    ${PORT} ${CONTAINER_DIR} 

# 下面是只运行容器的命令
# 目前 Arm64 架构的 Mac M1 芯片运行会有一些极其卡顿的问题，别问我怎么知道的（）
# docker run --rm -it -p ${PORT}:${PORT} -v \
    # "${HOST_DIR}:${CONTAINER_DIR}" \
    # reactorhttp-build \
    # /bin/bash

