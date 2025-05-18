#pragma once
#include <drogon/HttpFilter.h>
#include <jwt-cpp/jwt.h>

using drogon::HttpRequestPtr;
using drogon::FilterCallback;
using drogon::FilterChainCallback;
using drogon::HttpResponse;
using drogon::HttpStatusCode;

// JwtFilter 鉴权中间件，继承自 drogon::HttpFilter<JwtFilter>
class JwtFilter : public drogon::HttpFilter<JwtFilter, false> {
public:
    /// @brief 
    /// @param req 用于获取请求头信息
    /// @param fcb 用于令牌验证失败时返回响应（filter callback 的简写）
    /// @param fccb 用于令牌验证成功时继续将请求传递给下一个过滤器或最终的请求处理逻辑（filter chain callback 的简写）
    void doFilter(const HttpRequestPtr& req, 
                FilterCallback&& fcb, 
                FilterChainCallback&& fccb ) override {
        // 从请求头中获取 jwt 令牌，它被存储在请求头的 Authorization 字段中
        auto token = req->getHeader("Authorization");
        if(token.empty())
        {
            // 返回 401 错误，表示缺少令牌，无法进行身份验证
            sendError(fcb, 401, "Missing token");
            return;
        }

        try {
            // 解码 jwt 令牌
            auto decoded = jwt::decode(token);
            // 注册 jwt 验证器
            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{config_.secretKey})
                .with_issuer("neta-bilibili");

            // 进行 JWT 验证
            verifier.verify(decoded);
            // 将用户 ID 添加到请求头中
            // 过滤器把解码后的用户 ID 添加到请求头中，以便后续的处理逻辑可以使用它
            // 而无需再次解码 JWT 令牌
            req->addHeader("X-User-Id", decoded.get_subject());

            // 验证通过，继续处理请求
            // 如果验证链中不止有一个 JwtFilter，那么 fccb() 会调用下一个过滤器的 doFilter() 方法
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