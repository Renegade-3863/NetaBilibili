#pragma once
#include <drogon/HttpFilter.h>
#include <jwt-cpp/jwt.h>

using drogon::HttpRequestPtr;
using drogon::FilterCallback;
using drogon::FilterChainCallback;
using drogon::HttpResponse;
using drogon::HttpStatusCode;

class JwtFilter : public drogon::HttpFilter<JwtFilter> {
public:
    void doFilter(const HttpRequestPtr& req, 
                FilterCallback&& fcb, 
                FilterChainCallback&& fccb ) override {
        auto token = req->getHeader("Authorization");
        if(token.empty())
        {
            // 返回 401 错误，表示缺少令牌，无法进行身份验证
            sendError(fcb, 401, "Missing token");
            return;
        }

        try {
            // 解析 jwt 令牌
            auto decoded = jwt::decode(token);
            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{config_.secretKey})
                .with_issuer("neta-bilibili");
            
            verifier.verify(decoded);
            req->addHeader("X-User-Id", decoded.get_subject());

            // 验证通过，继续处理请求
            fccb();
        } catch (...) {
            // 返回 401 错误，表示无法正确验证用户身份
            sendError(fcb, 401, "Invalid token");
        }
    }
private:
    struct JwtConfig {
        std::string secretKey;
    } config_;
    
    void sendError(FilterCallback& cb, int code, const std::string& msg) {
        Json::Value json;
        json["code"] = code;
        json["message"] = msg;
        // 创建 Json 响应
        auto res = HttpResponse::newHttpJsonResponse(json);
        // 设置响应状态码为 code
        res->setStatusCode(static_cast<HttpStatusCode>(code));
        // 调用回调函数，将响应发送给客户端
        cb(res);
    }
};