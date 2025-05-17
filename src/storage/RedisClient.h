#pragma once
#include <hiredis/hiredis.h>
#include <mutex> 

class RedisClient {
public:
    RedisClient(const std::string& host, int port) {
        context_ = redisConnect(host.c_str(), port);
    }

    void setex(const std::string& key, int ttl, const std::string& value) {
        std::lock_guard lock(mutex_);
        redisCommand(context_, "SETEX %s %d %s", key.c_str(), ttl, value.c_str());
    }

private:
    redisContext* context_;
    std::mutex mutex_;
};