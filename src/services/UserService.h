#pragma once
#include <drogon/HttpTypes.h>
#include <drogon/orm/DbClient.h>
#include "../models/User.h"
#include "../storage/MySQLClient.h"
#include "../storage/RedisClient.h"

using namespace drogon;

// 用户业务逻辑模块
// 封装了与用户相关的数据库操作和业务逻辑
// 如用户注册、用户登录、用户信息查询等
// 与 HTTP 请求和响应无关，只负责处理业务逻辑
class UserService {
public:
    UserService(const MySQLClientPtr& mysql, const RedisClientPtr& redis) 
        : mysql_(mysql), redis_(redis) {}

private:
    MySQLClientPtr mysql_;
    RedisClientPtr redis_;
};