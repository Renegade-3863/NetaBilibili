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
// using namespace drogon::orm;

// 目前对应的路由处理函数还未实现，所以这里先注释掉
// class UserController : public HttpController<UserController> {
// public:
//     METHOD_LIST_BEGIN
//     // 注册路由
//         ADD_METHOD_TO(UserController::registerUser, "/api/user/register", Post);
//         ADD_METHOD_TO(UserController::login, "/api/user/login", Post);
//     METHOD_LIST_END
//     // 用户注册
//     void registerUser(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr& )>&& callback);

//     // 用户登陆（签发 JWT）
//     void login(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback);
// };