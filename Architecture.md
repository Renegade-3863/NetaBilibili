# 基于 Drogon 架构的仿 Bilibili 视频网站项目
## 技术栈
- C++
- Drogon 框架
- MySQL
- Redis
- ORM：Drogon ORM
- FFmpeg 视频处理
- JWT 认证
- Nginx 反向代理，负载均衡

## 项目架构图
``` mermaid
graph TD
A[客户端] --> |HTTP/WebSocket| B[nginx]
B --> C{功能}
C --> |视频上传| D[视频服务] 
C --> |API 路由| E[用户服务]
C --> |弹幕推送| F[WebSocket 网关]
D --> |转码任务| G[消息队列]
G --> |消费任务| H[FFmpeg Worker]
H --> |存储| I[对象存储]
E --> J{功能}
J --> |读写| K[MySQL]
J --> |缓存| L[Redis]
K --> |主从同步| M[MySQL Slave]
L --> |集群| N[Redis Node]
F --> O{功能}
O --> |弹幕时序存储| P[Redis]
O --> |广播弹幕| A
```
