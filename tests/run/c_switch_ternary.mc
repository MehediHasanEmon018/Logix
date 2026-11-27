#include <stdio.h>
#include <math.h>
#define MAX 5
#define RATE 2.5
#define CH 'x'
int grade(int m) { return m >= 80 ? 4 : m >= 60 ? 3 : 0; }
int main() {
    for (int i = -1; i <= MAX; i++) {
        switch (i) {
            case -1: printf("neg "); break;
            case 0: printf("zero "); continue;
            default: printf("d%d ", i);
            case 3: printf("three "); break;
            case 4: { int t = i * 2; printf("t%d ", t); }
        }
        printf("| ");
    }
    printf("\n%d %d %d\n", grade(90), grade(70), grade(10));
    float r = RATE * MAX;
    int k = (int) r;
    double half = (double) k / 2;
    printf("%.2f %d %.1f %c\n", r, k, half, CH);
    unsigned int u = 7; short s = 3; long int li = 9;
    printf("%d %d %d\n", u, s, li);
    int big = k > 10 ? k : -k;
    printf("%d %.3f %d\n", big, sqrt(2), abs(-big));
    char c = 'b';
    switch (c) { case 'a': printf("A\n"); break; case 'b': printf("B\n"); break; }
    int n = 0;
    while (1 == 1) { n++; switch (n % 3) { case 0: break; default: continue; } if (n > 7) break; }
    printf("n=%d\n", n);
    return 0;
}
