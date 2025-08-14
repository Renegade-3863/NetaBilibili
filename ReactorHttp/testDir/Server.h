#pragma once
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/epoll.h>

// 初始化监听的套接字
/***
 * @param port 指定初始化的套接字要监听的网卡的端口号
 * @return 监听的套接字文件描述符
 */
int initListenFd(unsigned short port);

// 启动 epoll 模型，同时也是主事件循环
/***
 * @param lfd 监听的套接字文件描述符
 * @return STATUS 返回一个状态值
 */
int epollRun(int lfd);

/**
 * @param lfd 服务器在监听的套接字文件描述符
 * @param epfd 用于管理 epoll 实例的文件描述符，我们要把这个 lfd 文件描述符交给 epfd 这个 epoll 对象进行管理
 * @return STATUS 返回一个状态值
 */
// 用于和客户端建立新连接的函数
void *acceptClient(void *arg);

/**
 * @param cfd 服务器端用于接收对应客户端数据的通信用文件描述符
 * @param epfd 用于管理 epoll 实例的文件描述符，这主要用于处理客户端发送完数据后就断开连接情况下，epoll 红黑树的相关节点删除需求
 * @return STATUS 返回一个状态值
 */
// 接收 http 请求
void *recvHttpRequest(void *arg);

/**
 * @param line 要解析的 http 请求的首行（一般对于 GET 请求，以请求行为主）
 * @param cfd 服务器端用于给对应客户端响应数据的通信用文件描述符
 * @return STATUS 返回一个状态值
 */
// 解析 http 请求
int parseRequestLine(const char* line, int cfd);

/**
 * @param filename 要发送的文件的文件名
 * @param cfd 服务器端用于给对应客户端响应数据的通信用文件描述符
 * @return STATUS 返回一个状态值
 */
// 发送文件
int sendFile(const char *filename, int cfd);

/**
 * @param cfd 与客户端建立连接的通信用套接字
 * @param status 要返回的 Http 状态码（200、301、404、500 等）
 * @param descr Http 响应头的内容（description）
 * @param type 返回的文件的文件类型描述（字符串形式的描述）
 * @param length 描述响应体的长度值
 */
// 发送 Http 响应头（状态行+响应头内容）
int sendHeadMsg(int cfd, int status, const char *descr, const char *type, int length);

/** 
 * @param dirname 要发送的目录的名称
 * @param cfd 与客户端建立连接的通信用套接字
 */
// 发送目录
int sendDir(const char *dirName, int cfd);

/**
 * @param name 文件的名称
 */
// 获取文件的类型信息
const char *getFileType(const char *name);