/*
    用户业务逻辑模块
*/

#pragma once
#include <drogon/HttpController.h>
#include <drogon/orm/DbClient.h>
#include "../services/UserService.h"
#include "../services/CryptoService.h"
#include "../models/User.h"
#include <json/json.h>

using namespace drogon;
using namespace drogon::orm;

// class UserController : public HttpController<UserController> {
// public:
//     METHOD_LIST_BEGIN
//         ADD_METHOD_TO(UserController::registerUser, )
//     METHOD_LIST_END

//     // 用户注册
//     void registerUser(const HttpRequestPtr& req,
//                       std::function<void(const HttpResponsePtr&)>&& callback) {
//         Json::Value json;
//         if(!parseJsonRequest(req, json, callback)) return;
//     }

// private:
//     std::shared_ptr<UserService> userService_;
// };