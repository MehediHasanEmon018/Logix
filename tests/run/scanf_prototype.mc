#include <stdio.h>
int power(int b, int e);
int isPrime(int n) {
    if (n < 2) return 0;
    for (int i = 2; i * i <= n; i++) if (n % i == 0) return 0;
    return 1;
}
int counter = 0;
void inc() { counter++; }
int main() {
    float x;
    char c;
    scanf("%f", &x);
    scanf(" %c", &c);
    printf("x=%.2f c=%c\n", x, c);
    for (int i = 1; i <= 20; i++) if (isPrime(i) == 1) printf("%d ", i);
    printf("\n");
    inc(); inc(); inc();
    printf("counter=%d power=%d\n", counter, power(2, 10));
    return 0;
}
int power(int b, int e) {
    int r = 1;
    while (e > 0) { r *= b; e--; }
    return r;
}
