#pragma once
#include <drogon/HttpTypes.h>
#include "../models/User.h"
#include "../storage/MySQLClient.h"
#include "../storage/RedisClient.h"

// class UserService {
// public:
//     UserService(const MySQLClientPtr& mysql, const RedisClientPtr& redis) 
//         : mysql_(mysql), redis_(redis) {}

// private:
//     MySQLClientPtr mysql_;
//     RedisClientPtr redis_;
// };