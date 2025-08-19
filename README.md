## 一个刚刚开发了三成不到的网络核心的（或许会是）仿造 B 站的网页平台（希望能做出来）

- 基本原理不算太难，就是 epoll + 线程池做的 Reactor 模型
- 目前进度：
  - TCP 基本架构实现了 LT 模式（后面可能会考虑迭代为 ET 模式），较大视频流实现了异步发送和零拷贝发送（**Linux sendfile** 技术）
  - 由于某些项目的原因，应用层实现了一个 WebSocket 通信模块，所以理论上来说是可以支持 WebSocket 通信的，但是路由接口目前只有回显功能（后续有需要再开发吧..）
  - 应用层主要的功能为显示文件和基本的文件夹跳转，后续会考虑全面升级为 sendfile 零拷贝发送
  - 前端暂时还未想好如何设计，可能会考虑使用 AI 来做（GPT-5 赛高！）
  - 关于可以在线预览的文件类型，可以在 HttpRequest.c 中检索定义了 MIMO 类型的相关函数

- 下一步目标：
    1. 让 GPT-5 帮我设计一个好看的前端界面！！！
    2. 实现文件上传功能，这是 B 站 UP 主们必须要用到的功能
    3. 至于按钮和后端功能的接口，目前还没想好怎么实现，先把前端搞出来再说


- 基本的运行方法：
    - 这里本人推荐使用仓库内打包好的 dockerfile 文件来构建本地的 Ubuntu 镜像
    - 备注：如果你本身就是 Linux 系统，或者用了 Windows 的 WSL 子系统，那么也可以直接克隆到 Linux 环境下执行 cmake 相关指令
- 运行步骤
    如果考虑用本地 Linux 环境，请跳转到步骤 2
    1. 构建镜像
    ``` bash
        # 进入克隆好的项目目录
        # 强烈建议习惯用 pushd，cd 也可以，但不如 pushd 方便（）
        pushd ./NetaBilibili/ReactorHttp
        docker build -t <你起的 docker 镜像名字>
    ```
    2. 使用克隆好的 .sh 脚本运行 docker 容器或直接一步到位启动服务器 
    （注意，别忘了把脚本中的本机目录路径改成你自己 Linux 环境下的目录，这个目录会挂载到容器内的 /app/test 目录）
    ``` bash
        ./runDocker.sh
    ```
    3. 一定要看这一条！！！
    ``` bash
        # 注意，需要把脚本文件中运行的镜像名字改成你上面起的名字：
        docker run --rm -it -p ${PORT}:${PORT} -v \
        "${HOST_DIR}:${CONTAINER_DIR}" \
        <你起的 docker 镜像名字> \
        /app/build/ReactorServer \
        ${PORT} ${CONTAINER_DIR} 
        # 脚本文件内可以修改运行方式，看一下注释就行
    ```
