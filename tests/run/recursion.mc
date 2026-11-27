#include <stdio.h>
int isEven(int n);
int isOdd(int n) { if (n == 0) return 0; return isEven(n - 1); }
int isEven(int n) { if (n == 0) return 1; return isOdd(n - 1); }
void fillrec(int a[], int i, int n) {
    int tmp[2] = {i, i * 10};
    if (i >= n) return;
    a[i] = tmp[1];
    fillrec(a, i + 1, n);
    printf("%d,%d ", tmp[0], tmp[1]);
}
int ack(int m, int n) {
    if (m == 0) return n + 1;
    if (n == 0) return ack(m - 1, 1);
    return ack(m - 1, ack(m, n - 1));
}
int main() {
    int a[4];
    fillrec(a, 0, 4);
    printf("\n%d %d %d %d\n", a[0], a[1], a[2], a[3]);
    printf("%d %d\n", isEven(10), isOdd(7));
    printf("%d\n", ack(2, 3));
    return 0;
}
