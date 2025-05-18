#include <iostream>
#include <type_traits>
// 可以使用 #pragma pack(n) 来指定对齐方式，其中 n 是一个正整数，表示对齐字节数。
// #pragma pack(4)

class nonsense {
};

class align_without_static
{
public:
    align_without_static() {}
    virtual ~align_without_static() {}
    int a;
    // double b;
    int c;
};

class align_with_static
{
public:
    align_with_static() {}
    virtual ~align_with_static() {}
    int a;
    // 静态成员变量不占用类对象的空间，它定义在程序的全局/静态数据区。
    static double b;
    int c;
};

int main() 
{
    std::cout << "sizeof empty class: " << sizeof(nonsense) << std::endl;
    std::cout << "sizeof align_without_static: " << sizeof(align_without_static) << std::endl;
    std::cout << "sizeof align_with_static: " << sizeof(align_with_static) << std::endl;

    std::cout << "alignof empty class: " << alignof(nonsense) << std::endl;
    std::cout << "alignof align_with_static: " << alignof(align_with_static) << std::endl;
    std::cout << "alignof align_without_static: " << alignof(align_without_static) << std::endl;

    return 0;
}