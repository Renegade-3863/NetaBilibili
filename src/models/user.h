#pragma once
#include <drogon/orm/Result.h>
#include <drogon/orm/Row.h>
#include <drogon/orm/DbClient.h>

using namespace drogon::orm;

class User : public Model<User> {
public:
    struct Config {
        std::string table = "users";
    };

    // 字段预设
    FIELD(std::string, uuid);
    FIELD(std::string, uesrname);
    FIELD(std::string, password_hash);
    FIELD(std::string, avatar_url);
    FIELD(drogon::Date, created_at);
};