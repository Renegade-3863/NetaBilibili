/* 
    API 网关主程序
*/
#include <drogon/drogon.h>
#include "controllers/UserController.h"
#include "middleware/JwtFilter.h"

int main()
{
    // 加载 Drogon 配置文件
    drogon::app().loadConfigFile("../config.json");

    // 注册中间件（全局生效模式）
    drogon::app().registerFilter(std::make_shared<JwtFilter>());

    // 注册控制器
    drogon::app().registerController(std::make_shared<UserController>());
}
