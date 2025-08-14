#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "Server.h"

/**
 * @param argc 命令行调用的参数总数，包括指令本身
 * @param argv 命令行调用的参数列表
 */
int main(int argc, char *argv[])
{
    if(argc < 3)
    {
        // 参数总个数少于 3，说明调用不合法，我们提示用户修改调用方法
        printf("./a.out port path: ");
        return -1;
    }
    // 把当前服务器进程的工作目录修改为当前程序的工作目录
    chdir(argv[2]);

    // 需要把字符串类型的传入参数类型转换成整数再交给初始化函数
    unsigned short port = atoi(argv[1]);
    // 初始化用于监听的套接字
    int lfd = initListenFd(port);
    // 启动服务器程序
    epollRun(lfd);

    return 0;
}