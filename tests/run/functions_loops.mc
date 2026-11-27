#include <stdio.h>
int fact(int n) {
    if (n <= 1) return 1;
    return n * fact(n - 1);
}
int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}
int gcd(int a, int b) {
    while (b != 0) { int t = b; b = a % b; a = t; }
    return a;
}
void hello(int times) {
    for (int i = 0; i < times; i++) printf("hello %d\n", i);
}
float avg(float x, float y) { return (x + y) / 2; }
int main() {
    printf("%d %d %d\n", fact(5), fib(15), gcd(48, 18));
    hello(3);
    printf("%.2f\n", avg(3, 4));
    int i = 0;
    do { printf("do %d\n", i); i++; } while (i < 3);
    for (int k = 0; k < 10; k++) {
        if (k == 2) continue;
        if (k == 6) break;
        printf("k=%d\n", k);
    }
    int s = 0, j = 0;
    while (1 == 1) { j++; if (j % 2 == 0) continue; if (j > 9) break; s += j; }
    printf("s=%d j=%d\n", s, j);
    for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++) { if (b == 1) continue; if (a == 2) break; printf("%d%d ", a, b); }
    printf("\n");
    return 0;
}
