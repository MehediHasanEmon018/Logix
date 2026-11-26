#include <stdio.h>
#include <math.h>
#define N 5
#define PI 3.14159

int fact(int n) {
    if (n <= 1) return 1;
    return n * fact(n - 1);
}
int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
void printArr(int a[], int n) {
    for (int i = 0; i < n; i++) printf("%d ", a[i]);
    printf("\n");
}
void bubble(int a[], int n) {
    for (int i = 0; i < n - 1; i++)
        for (int j = 0; j < n - i - 1; j++)
            if (a[j] > a[j + 1]) { int t = a[j]; a[j] = a[j + 1]; a[j + 1] = t; }
}
float avg(int a[], int n) {
    int s = 0;
    for (int i = 0; i < n; i++) s += a[i];
    return (float) 0 + s / (n * 1.0);
}
int isPrime(int n);
int count = 0;

int main(void) {
    int a[N] = {5, 3, 9, 1, 7};
    long long big = 100000;
    const int k = 3;
    char grade = 'B';
    printArr(a, N);
    bubble(a, N);
    printArr(a, N);
    printf("avg = %.2f\n", avg(a, N));
    printf("5! = %d, fib(10) = %d\n", fact(5), fib(10));
    printf("sqrt(16)=%.1f pow(2,10)=%.0f abs(-4)=%d area=%.3f\n", sqrt(16), pow(2, 10), abs(-4), PI * k * k);

    int i = 0;
    do { i++; if (i == 2) continue; if (i > 6) break; printf("%d,", i); } while (i < 10);
    printf("\n");
    for (int x = 1; x <= 20; x++) {
        if (x % 2 == 0) continue;
        if (x > 9) break;
        count++;
    }
    printf("odd count = %d\n", count);

    switch (grade) {
        case 'A': printf("Excellent\n"); break;
        case 'B':
        case 'C': printf("Good %c\n", grade); break;
        default: printf("??\n");
    }
    int marks = 72;
    if (marks >= 80) printf("A+\n");
    else if (marks >= 70) printf("A\n");
    else printf("F\n");
    for (int p = 2; p <= 20; p++) if (isPrime(p) == 1) printf("%d ", p);
    printf("\nbig=%d 100%%\n", big);
    int m[3];
    m[0] = 1; m[1] += 5; m[2]++;
    printf("%d %d %d\n", m[0], m[1], m[2]);
    return 0;
    printf("never\n");
}
int isPrime(int n) {
    if (n < 2) return 0;
    for (int i = 2; i * i <= n; i++) if (n % i == 0) return 0;
    return 1;
}
