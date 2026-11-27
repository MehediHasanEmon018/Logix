#include <iostream>
#include <iomanip>
using namespace std;

bool isEven(int n) { return n % 2 == 0; }

int main() {
    int a, b;
    double d;
    char op;
    cin >> a >> b >> d >> op;
    cout << "a+b=" << a + b << ", a/b=" << a / b << endl;
    cout << "d=" << d << " d/3=" << d / 3 << " 10.0=" << 10.0 << endl;
    cout << "even? " << isEven(a) << " op=" << op << " " << 'Z' << endl;
    bool ok = a > b;
    cout << ok << endl;
    switch (op) {
        case '+': cout << "plus" << endl; break;
        default: cout << "other" << endl;
    }
    for (int i = 1; i <= 3; i++) {
        for (int j = 0; j < i; j++) cout << "*";
        cout << "\n";
    }
    int big = a > b ? a : b;
    std::cout << "max " << big << std::endl;
    cout << fixed << setprecision(2) << d << " " << (double) a / b << endl;
    return 0;
}
