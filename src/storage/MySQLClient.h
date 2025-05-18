#pragma once
#include <drogon/orm/DbClient.h>

// 使用 drogon 框架提供的 DbClient 类型实现与 MySQL 数据库的连接
using MySQLClientPtr = std::shared_ptr<drogon::orm::DbClient>;
