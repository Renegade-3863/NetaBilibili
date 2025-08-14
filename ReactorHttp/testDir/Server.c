#define _POSIX_C_SOURCE 199309L
#define _DEFAULT_SOURCE

#include "Server.h"
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <memory.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>

/**
 * @param fd 要用于建立连接的监听文件描述符
 * @param epfd 用于管理 epoll 实例的文件描述符
 * @param tid 要新建的线程的 id 信息
 */
// 用于 acceptClient 的参数的结构体定义
struct FdInfo
{
    int fd;
    int epfd;
    pthread_t tid;
};

/*
    定义一组 static 的解码函数，由于汉字在 Linux 中文件系统中传输的时候会被强制类型转换成 UTF-8 编码格式
    所以我们需要在服务器端接收到文件名后对其进行手动转换
*/

/**
 * @param c 16 进制的单个字符
 */
// 将 16 进制的 UTF-8 字符转换成 10 进制的数字的函数
static int hexToDec(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 0;
}

/**
 * @param to 解码后的字符串
 * @param from 编码后、解码前的字符串
 */
// 解码函数
static void decodeMsg(char *to, char *from)
{
    // 遍历要转换的字符串，直到到达 from 字符串的结尾
    for (; *from != '\0'; ++to, ++from)
    {
        // isxdigit 函数用于判断传入的字符是不是 16 进制格式，参数取值为 0~9 a~f A~F
        // 含 UTF-8 编码的文件名举例：
        // Linux%E5%86%85%E6%A0%B8.jpg
        // 其中 % 后面的两个字符是 16 进制的编码
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
        {
            // 如果上面的条件成立，说明 from 的前三个字符构成了一个 UTF-8 编码的字符
            // 我们需要对其进行转换
            *to = hexToDec(from[1]) * 16 + hexToDec(from[2]);

            // 转换完成后，to 指针就指向了 from 原本指向的 UTF-8 编码实际对应的字符值（10 进制）
            // 为了统一上面 for 循环语句第三段 ++from 的逻辑，我们这里只让 from 先移动 2 个字节
            from += 2;
        }
        else
        {
            // 否则，说明 from 的前三个字符都是正常的 ASCII 字符
            // 我们直接拷贝即可
            *to = *from;
        }
    }
    // 截断字符串，防止读取到错误的数据
    *to = '\0';
}

int initListenFd(unsigned short port)
{
    // 1. 设置监听的文件描述符（fd）
    // 使用 Ipv4 的 TCP 套接字，TCP 是流式协议，所以用 SOCK_STREAM
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1)
    {
        perror("socket");
        return -1;
    }
    // 2. 设置端口复用
    int opt = 1;
    int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret == -1)
    {
        perror("setsockopt");
        return -1;
    }
    // 3. 绑定端口号
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    // 设置 0 地址，表示可以绑定本机所有的 IP 地址
    addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(lfd, (struct sockaddr *)(&addr), sizeof(addr));
    if (ret == -1)
    {
        perror("bind");
        return -1;
    }
    // 4. 设置监听
    ret = listen(lfd, 128);
    // 5. 返回监听的文件描述符
    return lfd;
}

int epollRun(int lfd)
{
    // 1. 创建 epoll 实例
    int epfd = epoll_create(1);
    if (epfd == -1)
    {
        perror("epoll_create");
        return -1;
    }
    struct epoll_event ev;
    // 指定 epoll 管理监听文件描述符
    ev.data.fd = lfd;
    // 监听的文件描述符只需要监听读事件
    ev.events = EPOLLIN;
    // 2. 把监听的文件描述符添加到 epoll 实例中
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
    if (ret == -1)
    {
        perror("epoll_ctl");
        return -1;
    }
    // 3. 使用内核代码检测事件
    // 这是主事件循环
    // 指定我们要一次检测最多 1024 个事件
    struct epoll_event evs[1024];
    while (1)
    {
        int size = sizeof(evs) / sizeof(struct epoll_event);
        // epoll_wait 的返回值为本次调用检测到的事件数量
        // 最后一个参数为 -1 表示一直阻塞，为 0 表示不阻塞，为大于 0 的数表示阻塞的时间
        int num = epoll_wait(epfd, evs, size, -1);
        for (int i = 0; i < num; ++i)
        {
            struct FdInfo *info = (struct FdInfo*)malloc(sizeof(struct FdInfo));
            info->fd = evs[i].data.fd;
            info->epfd = epfd;
            // 4. 处理事件
            // 判断当前遍历到的文件描述符是否是监听的文件描述符
            int fd = evs[i].data.fd;
            if (fd == lfd)
            {
                // acceptClient(fd, epfd);
                // 说明这是一个监听描述符
                // 那么我们需要调用 accept 函数完成连接过程（注意 accept 是取的全连接队列中的连接返还给操作系统
                // 对 acceptClient 的两个参数进行了封装
                pthread_create(&info->tid, NULL, acceptClient, info);
            }
            else
            { 
                // 说明这是一个常规的事件，不是新连接请求
                // 主要是处理读事件
                // recvHttpRequest(fd, epfd);
                pthread_create(&info->tid, NULL, recvHttpRequest, info);
            }
        }
    }
    return 0;
}

void *acceptClient(void *arg)
{
    struct FdInfo *info = (struct FdInfo*)arg;
    // 1. 建立新连接
    // 第二个参数用于保存连接到本服务器的客户端的地址和端口信息，第三个参数为对应地址结构体的长度
    // 如果不需要这个信息，可以全部指定为 NULL
    int cfd = accept(info->fd, NULL, NULL);
    if (cfd == -1)
    {
        perror("accept");
        return NULL;
    }
    // 2. 设置要监听的文件描述符的属性，为边缘非阻塞模式
    // 先用 fcntl 函数取出当前监听的客户端文件描述符的属性值
    int flag = fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);

    // 3. 把 cfd 描述符添加到 epoll 模型中，让它进行检测
    struct epoll_event ev;
    ev.data.fd = cfd;
    ev.events = EPOLLIN | EPOLLET;
    int ret = epoll_ctl(info->epfd, EPOLL_CTL_ADD, cfd, &ev);
    if (ret == -1)
    {
        perror("epoll_ctl");
        return NULL;
    }
    printf("acceptClient threadId: %ld\n", pthread_self());
    // 对应外面的 info 结构体，在当前线程使用完毕后就可以被释放掉了
    free(info);
    return NULL;
}

void *recvHttpRequest(void *arg)
{
    struct FdInfo *info = (struct FdInfo*)arg;
    // printf("开始接收数据了...\n");
    // 创建一个 buffer，用于处理数据
    // 初始化为全 0
    char buf[4096] = { 0 };
    // 另创建一个 tmp 缓存，处理无法一次性接收完数据的情况
    // 如果不使用一个临时数组，可能会出现数据覆盖问题
    char tmp[1024] = { 0 };
    int len = 0, total = 0;
    
    while ((len = recv(info->fd, tmp, sizeof(tmp), 0)) > 0)
    {
        if (total + len < (int)sizeof(buf))
        {
            memcpy(buf + total, tmp, len);
        }
        total += len;
    }
    // 判断数据是否被接收完毕
    if (len == -1 && errno == EAGAIN)
    {
        // 对请求行进行解析
        // 最开始的部分是请求头，我们需要把它单独提取出来
        // 找到第一个 \r\n 结尾，这是 html 协议中的行尾标记
        char *pt = strstr(buf, "\r\n");
        int reqLen = pt - buf;  // 获取请求头的总字节长度
        buf[reqLen] = '\0';     // 截断整个请求字段
        // 对第二行（HTTP 协议规定的请求行）进行解析
        parseRequestLine(buf, info->fd);
    }
    else if (len == 0)
    {
        // 此时 recv 返回 0，说明客户端已经断开了连接
        // 那么我们需要把这个文件描述符从 epoll 模型中删除
        epoll_ctl(info->epfd, EPOLL_CTL_DEL, info->fd, NULL);
        // 关闭文件描述符
        close(info->fd);
    }
    else
    {
        // 出错了，打印错误信息
        perror("recv");
    }
    printf("recvMsg threadId: %ld\n", pthread_self());
    free(info);
    return NULL;
}

int parseRequestLine(const char* line, int cfd)
{
    // 解析请求行
    /*
        e.g. 
        get /xxx/1.jpg http/1.1
    */
    // 两种方案，一种是进行常规遍历，根据空格进行手动解析
    // 另一种是使用 sscanf 函数进行字符串进行格式化解析
    // 我们这里使用第二种方案进行处理
    char method[12];    // 常用的一般就是 "GET" 和 "POST"
    char path[1024];    // 一般就是请求的资源路径

    // 进行格式化解析
    sscanf(line, "%[^ ] %[^ ]", method, path);
    
    // printf("method: %s, path: %s\n", method, path);
    if (strcasecmp(method, "get") != 0)
    {
        // 说明不是 GET 
        return -1;
    }
    // 转换解析出来的文件名
    // 之所以可以自调用，是因为解码函数中，to 指针指向的相对位置一定一直在 from 指针前面
    decodeMsg(path, path);
    printf("Decoded path is: %s\n", path);
    // 否则，说明收到了一个 GET 请求
    // 我们处理客户端要求的静态资源（目录/文件）
    char *file = NULL;
    // 修改路径，方便后续使用目录路径
    if (strcmp(path, "/") == 0)
    {
        file = "./";
    }
    else
    {
        // 指针后移一个字节，丢弃开头的 "/" 即可
        file = path + 1;
    }
    // printf("\n file is: %s\n", file);
    // 获取文件属性
    // 这个结构体定义在 <sys/stat.h> 中
    struct stat st;
    int ret = stat(file, &st);
    if (ret == -1)
    {
        // 获取文件属性信息不存在，也就说明文件本身就不存在
        // 那么我们需要返回一个 404 错误
        sendHeadMsg(cfd, 404, "Not Found", getFileType(".html"), -1);
        sendFile("404.html", cfd);

        return 0;
    }
    // 判断文件类型
    if (S_ISDIR(st.st_mode))
    {
        // 说明是一个目录
        // 那么我们把这个目录中的内容返还给客户端
        sendHeadMsg(cfd, 200, "OK", getFileType(".html"), -1);
        sendDir(file, cfd);
    }
    else
    {
        // 说明是一个文件
        // 那么我们把这个文件的内容返还给客户端
        sendHeadMsg(cfd, 200, "OK", getFileType(file), st.st_size);
        sendFile(file, cfd);
    }

    return 0;
}

int sendFile(const char *filename, int cfd)
{
    // 1. 打开要传输的文件
    // 以只读方式打开，防止文件被误修改
    printf("filename: %s\n", filename);
    int fd = open(filename, O_RDONLY);
    // 使用断言判断文件是否打开成功
    // assert(fd > 0);
#if 0
    // 使用一个 while 循环，不断地从文件中读取一定的数据并经 TCP 连接进行发送
    while (1)
    {
        char buf[1024];
        int len = read(fd, buf, sizeof(buf));
        if (len > 0)
        {
            send(cfd, buf, len, 0);
            // 防止发送端发送数据的频率过快，充满了接收端的缓存
            // 这里我们让发送线程每次发送保持一些间隔
            // timespec 结构体用于毫秒级别的时间操作
            struct timespec ts = {
                .tv_sec = 0,
                .tv_nsec = 10 * 1000000L
            };
            nanosleep(&ts, NULL);
        }
        else if (len == 0)
        {
            // 读取已经完成，可以退出循环
            break;
        }
        else
        {
            perror("read");
        }
    }
#else   
    off_t offset = 0;
    int size = lseek(fd, 0, SEEK_END);
    // 上面一行代码会把对应 fd 文件的读取指针移到文件末尾，我们需要把文件指针再移回来
    lseek(fd, 0, SEEK_SET);
    // 当文件没有发送完成时，重复调用 sendfile 函数进行数据发送
    while(offset < size)
    {
        // 注意动态更新要发送的数据块的大小，根据函数自己更新的 offset 值对 size 进行修改
        int ret = sendfile(cfd, fd, &offset, size-offset);
        // if (ret == -1 && errno == EAGAIN)
        // {
        //     printf("没数据...\n");
        // }
        if (ret == -1 && errno != EAGAIN)
        {
            // 不是读取缓存与向缓存中写入数据速率之间的差异问题
            // 那就是出错了，我们此时结束函数即可
            return -1;
        }
    }
#endif
    close(fd);
    return 0;
}

int sendHeadMsg(int cfd, int status, const char *descr, const char *type, int length)
{
    // 构建状态行缓存
    char buf[4096] = { 0 };
    sprintf(buf, "http/1.1 %d %s\r\n", status, descr);
    // 构建响应头
    sprintf(buf + strlen(buf), "content-type: %s\r\n", type);
    sprintf(buf + strlen(buf), "content-length: %d\r\n\r\n", length);

    send(cfd, buf, strlen(buf), 0);

    return 0;
}

/* 样例 HTML 文件
<html>
    <head>
        <title>test</title> 
    </head>
    <body> 
        <table> 
            <tr> 
                <td></td> 
                <td></td> 
            </tr> 
            <tr> 
                <td></td> 
                <td></td> 
            </tr>
        </table> 
    </body> 
</html>
*/
int sendDir(const char *dirName, int cfd)
{
    char buf[4096] = { 0 };
    // 拼接 html 文件的 head 字段
    sprintf(buf, "<html><head><title>%s</title></head><body><table>", dirName);
    struct dirent** namelist;
    // 不需要排序筛选规则，第三个参数指定为 NULL
    // 扫描目录的顺序遵循 Linux 自定义的 alphasort 方式，即文件名字母顺序的模式
    int num = scandir(dirName, &namelist, NULL, alphasort);
    for (int i = 0; i < num; ++i)
    {
        // 取出一个文件名
        // 这里 namelist 指向的是一个指针数组 struct dirent *tmp
        char *name = namelist[i]->d_name;
        // 判断取出来的 name 代表的是一个更小的子目录，还是一个正常的文件
        struct stat st;
        char subPath[1024] = { 0 };
        sprintf(subPath, "%s/%s", dirName, name);
        stat(subPath, &st);
        // printf("\nSubpath is: %s", subPath);
        if (S_ISDIR(st.st_mode))
        {
            // 添加链接，使用 <a></a> 标签
            // 语法为 <a href="">name</a>
            sprintf(buf+strlen(buf), 
                    "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",
                     name, name, st.st_size);
        }
        else
        {
            sprintf(buf+strlen(buf), 
                    "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
                     name, name, st.st_size);            
        }
        send(cfd, buf, strlen(buf), 0);
        // 清空发送缓存
        memset(buf, 0, sizeof(buf));
        // 内存清理
        free(namelist[i]);
    }
    sprintf(buf, "</table></body></html>");
    send(cfd, buf, strlen(buf), 0);
    // 清理二级指针
    free(namelist);
    return 0;
}

const char *getFileType(const char *name)
{
    // a.jpg a.mp4 a.html
    // 自右向左查找 '.' 字符，如不存在返回 NULL
    // 自右向左，防止文件名本身就包含点号
    const char *dot = strrchr(name, '.');
    printf("File Extension: %s\n", dot);
    if (dot == NULL)
        return "text/plain; charset=utf-8";     // 纯文本
    if (strcmp(dot, ".pdf") == 0)
        return "application/pdf";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp(dot, ".wav") == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg"; 
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";
    if (strcmp(dot, ".mp4") == 0)
        return "video/mp4";
    if (strcmp(dot, ".json") == 0)
        return "application/json";

    return "text/plain; charset=utf-8";
}
