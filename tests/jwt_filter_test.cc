// #include <gtest/gtest.h>
// #include <gmock/gmock.h>
// #include <drogon/HttpRequest.h>
// #include <drogon/HttpResponse.h>
// #include <jwt-cpp/jwt.h>
// #include "../src/middleware/JwtFilter.h"

// using namespace drogon;
// using ::testing::_;
// using ::testing::Return;
// using ::testing::ReturnRef;

// // 模拟 HttpRequest 类，用于模拟测试 HTTP 请求依赖
// class MockRequest : public HttpRequest 
// {
// public:
//     // 模拟获取请求头的 getHeader 方法
//     // getHeader 接受一个 std::string 类型的参数，返回一个 const std::string& 类型的值
//     // MOCK 系列模拟器的参数设置要与被模拟方法一致
//     MOCK_CONST_METHOD1(getHeader, const std::string&(std::string));
//     // 模拟添加请求头的 addHeader 方法
//     MOCK_METHOD2(addHeader, void(std::string, std::string&&));
// };

// // 编写 JwtFilter 的测试类
// class JwtFilterTest : public ::testing::Test 
// {
// protected:
//     // 在每个测试用例之前执行的初始化操作
//     void SetUp() override 
//     {
//         // 初始化一个 filter 对象
//         filter_ = std::make_shared<JwtFilter>();
//     }

//     std::shared_ptr<JwtFilter> filter_;
//     // 配置一个有效的 jwt 令牌
//     std::string validToken = jwt::create()
//         .set_issuer("neta-bilibili")
//         .set_subject("user123")
//         .sign(jwt::algorithm::hs256{"test-secret"});
// };

// // 第一个单元测试用例
// // 测试场景：请求头中缺少令牌
// // 预期结果：返回 401 错误，表示缺少令牌，无法进行身份验证
// TEST_F(JwtFilterTest, MissingTokenTest) {
//     auto req = std::make_shared<MockRequest>();
//     const std::string empty_token;
//     EXPECT_CALL(*req, getHeader("Authorization")).WillOnce(ReturnRef(empty_token));

//     testing::MockFunction<void(const HttpResponsePtr&)> mockFcb;
//     testing::MockFunction<void()> mockFccb;

//     EXPECT_CALL(mockFccb, Call()).Times(0);
//     EXPECT_CALL(mockFcb, Call(testing::AllOf(
//         testing::Property(&HttpResponse::getStatusCode, HttpStatusCode::k401Unauthorized),
//         testing::Property(&HttpResponse::jsonObject, testing::ResultOf([](const Json::Value& json) {
//             return json["code"].asInt() == 401 && json["message"].asString() == "Missing token";
//         }, testing::IsTrue()))
//     )));

//     filter_->doFilter(req, mockFcb.AsStdFunction(), mockFccb.AsStdFunction());
// }


// // 第二个单元测试用例
// // 测试场景：请求头中包含无效的令牌
// // 预期结果：返回 401 错误，表示无法正确验证用户身份
// TEST_F(JwtFilterTest, InvalidTokenTest) 
// {
//     auto req = std::make_shared<MockRequest>();
//     const std::string invalid_token = "invalid.token.here";
//     EXPECT_CALL(*req, getHeader("Authorization")).WillOnce(ReturnRef(invalid_token));

//     testing::MockFunction<void(const HttpResponsePtr&)> mockFcb;
//     testing::MockFunction<void()> mockFccb;

//     EXPECT_CALL(mockFccb, Call()).Times(0);
//     EXPECT_CALL(mockFcb, Call(testing::AllOf(
//         testing::Property(&HttpResponse::getStatusCode, HttpStatusCode::k401Unauthorized),
//         testing::Property(&HttpResponse::jsonObject, testing::ResultOf([](const Json::Value& json) {
//             return json["code"].asInt() == 401 && json["message"].asString() == "Invalid token";
//         }, testing::IsTrue()))
//     )));

//     filter_->doFilter(req, mockFcb.AsStdFunction(), mockFccb.AsStdFunction());
// }

// // 第三个单元测试用例
// // 测试场景：请求头中包含有效的令牌
// // 预期结果：验证通过，继续处理请求
// TEST_F(JwtFilterTest, ValidTokenTest)
// {
//     auto req = std::make_shared<MockRequest>();
//     EXPECT_CALL(*req, getHeader("Authorization")).WillOnce(ReturnRef(validToken));
//     // addHeader 方法的第二个参数是 std::string&& 类型的，因此需要传入一个右值
//     EXPECT_CALL(*req, addHeader("X-User-Id", std::string("user123")));

//     testing::MockFunction<void(const HttpResponsePtr&)> mockFcb;
//     testing::MockFunction<void()> mockFccb;

//     EXPECT_CALL(mockFcb, Call(_)).Times(0);
//     EXPECT_CALL(mockFccb, Call());

//     filter_->doFilter(req, mockFcb.AsStdFunction(), mockFccb.AsStdFunction());
// }