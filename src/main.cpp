/* 
    API 网关主程序
*/
#include <drogon/drogon.h>
#include "controllers/UserController.h"
#include "middleware/JwtFilter.h"

int main()
{
    // 加载 Drogon 配置文件
    drogon::app().loadConfigFile("../config.json");

    // 注册中间件（全局生效模式）
    drogon::app().registerFilter(std::make_shared<JwtFilter>());

    // 配置数据库连接池
    MySQLClientPtr MySQLClient = drogon::orm::DbClient::newMysqlClient(
        "host=127.0.0.1 port=3306 dbname=netabilibili user=jiechengheren password=your_password",
        4
    );

    // 在初始化 MySQL 客户端后添加测试代码
    MySQLClient->execSqlAsync(
        "SELECT 1", // 简单查询验证数据库响应
        [](const drogon::orm::Result& r) {
            LOG_INFO << "✅ MySQL 连接成功";
        },
        [](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "❌ MySQL 连接失败: " << e.base().what();
        }
    );

    // 正确初始化 Redis 客户端
    RedisClientPtr redisClient = drogon::nosql::RedisClient::newRedisClient(
        trantor::InetAddress("127.0.0.1", 6379, false));

    redisClient->execCommandAsync(
    [](const drogon::nosql::RedisResult& r) {
        if (r.asString() == "PONG") {
            LOG_INFO << "✅ Redis 连接成功";
        } else {
            LOG_ERROR << "❌ Redis 异常响应: " << static_cast<int>(r.type());
        }
    },
        [](const std::exception& e) {
            LOG_ERROR << "❌ Redis 连接失败: " << e.what();
        },
        "PING"
    );
    // 设置 drogon 服务为在 localhost:8080 监听
    drogon::app().addListener("127.0.0.1", 8080);
    // 设置线程个数，本机为 8 核 CPU，故设置为 8
    drogon::app().setThreadNum(8);
    // 运行 drogon 服务
    drogon::app().run();

    return 0;
}
