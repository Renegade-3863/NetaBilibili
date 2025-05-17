#pragma once
#include <drogon/orm/DbClient.h>

class MySQLClient : public drogon::orm::DbClient {
public:
    template<typename... Args>
    void execSql(const std::string& sql,
                std::function<void(const Result&)>&& cb,
                Args&&... args) {
        execSqlAsync(
            sql,
            [cb](const Result& r) { cb(r); }
            [](const DrogonObException& e) {
                LOG_ERROR << "DB Error: " << e.what();
            },
            std::forward<Args>(args)...
        );
    }
};