# 开发日志
### 2025-05-12
- 配置项目基本依赖项
    - 检查了 VSCode 的 include 目录
        - 具体的，添加了 vcpkg 的 include 目录到 VSCode 到 c_cpp_properties.json 文件中（通用路径名为 ``${env:VCPKG_ROOT}/installed/arm64-osx/include``）
    - 创建了简单的用户服务模块架构
    ``` mermaid
    graph TD
        subgraph 客户端
        A[Web/App] --> |HTTP 请求| B[API 网关]
    end

    subgraph API 层
        B --> C[JWT 鉴权中间件]
        C --> |验证通过| D[用户控制器]
        C --> |无效令牌| E[返回 401 错误] 
    end

    subgraph 业务逻辑层
        D --> F[用户服务]
        F --> G[密码加密处理]
        F --> H[数据校验]
    end

    subgraph 数据层
        F --> I[(MySQL 用户表)]
        F --> J[(Redis 会话缓存)]
    end

    subgraph 基础设施
        K[BCrypt 加密] --> G
        L[JWT 签发/验证] --> C
        subgraph 监控扩展层
            M[Prometheus] --> |抓取指标| F
            N[Thanos Sidebar] --> M
            N --> |上传块数据| O[对象存储 S3]
            P[Thanos Querier] --> |查询| N
            P --> |查询历史数据| Q[Thanos Store Gateway]
            Q --> O
            R[Thanos Compactor] --> |压缩数据| O
        end
    end

    ```
    - 其中，401 错误码代表无法验证用户身份，故无法响应请求的资源
    - 与之类似的是 403 码，代表用户没有权限访问资源