#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include "TcpServer.h"

int main(int argc, char* argv[])
{
    //printf("Hello?");
    if (argc < 3)
    {
        // 参数总个数少于 3，说明调用不合法，我们提示用户修改调用方法
        printf("./a.out port path: ");
        return -1;
    }
    unsigned short port = atoi(argv[1]);
    // 把当前服务器进程的工作目录修改为当前程序的工作目录
    chdir(argv[2]);

    // 忽略 SIGPIPE，避免向已关闭 socket 写入时整个进程被终止
    signal(SIGPIPE, SIG_IGN);

    // 启动服务器实例
    struct TcpServer* server = tcpServerInit(port, 2000);
    //printf("TCP Server Thread Pool: %d\n", server->threadPool->isStart);
    tcpServerRun(server);


    return 0;
}