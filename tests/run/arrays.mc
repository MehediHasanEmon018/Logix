#include <stdio.h>
int sum(int a[], int n) {
    int s = 0;
    for (int i = 0; i < n; i++) s += a[i];
    return s;
}
void bump(int a[], int n) { for (int i = 0; i < n; i++) a[i] = a[i] * 2; }
int maxof(int a[], int n) {
    int m = a[0];
    for (int i = 1; i < n; i++) if (a[i] > m) m = a[i];
    return m;
}
int main() {
    int v[5] = {3, 9, 2, 7, 5};
    int w[] = {1, 2, 3};
    float f[3] = {1, 2.5, 3};
    printf("%d %d\n", sum(v, 5), maxof(v, 5));
    bump(v, 5);
    printf("%d %d %d\n", v[0], v[1], sum(v, 5));
    printf("%d\n", sum(w, 3));
    f[1] += 1;
    f[2]++;
    printf("%.1f %.1f %.1f\n", f[0], f[1], f[2]);
    int i, n = 5;
    for (i = 0; i < n - 1; i++)
        for (int j = 0; j < n - 1 - i; j++)
            if (v[j] > v[j + 1]) { int t = v[j]; v[j] = v[j + 1]; v[j + 1] = t; }
    for (i = 0; i < n; i++) printf("%d ", v[i]);
    printf("\n");
    return 0;
}
