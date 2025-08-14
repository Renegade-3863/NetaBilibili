#include <iostream>
#include <cmath>

using namespace std;

// 判断一个数是否为质数
bool isPrime(int num) {
    if (num <= 1) return false;
    if (num == 2) return true;
    if (num % 2 == 0) return false;
    
    for (int i = 3; i <= sqrt(num); i += 2) {
        if (num % i == 0) {
            return false;
        }
    }
    return true;
}

// 寻找两个质数，使它们的和等于给定的偶数
void findGoldbachPrimes(int evenNum) {
    if (evenNum <= 2 || evenNum % 2 != 0) {
        cout << "输入必须是大于2的偶数！" << endl;
        return;
    }
    
    for (int i = 2; i <= evenNum / 2; ++i) {
        if (isPrime(i) && isPrime(evenNum - i)) {
            cout << evenNum << " = " << i << " + " << (evenNum - i) << endl;
            return;
        }
    }
    
    cout << "未找到符合条件的质数对！" << endl;
}

int main() {
    int number;
    
    cout << "请输入一个大于2的偶数: ";
    cin >> number;
    
    findGoldbachPrimes(number);
    
    return 0;
}