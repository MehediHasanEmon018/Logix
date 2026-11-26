int total = 0;
int x = 10;
void addTo(int v) { int x = v * 2; total += x; }
int sumArr(int a[], int n) { if (n == 0) return 0; return a[n-1] + sumArr(a, n - 1); }
void fill(int a[], int n) { for (int i = 0; i < n; i++) a[i] = i * i; }
int main() {
    int x = 3;
    addTo(x); addTo(5);
    int b[6];
    fill(b, 6);
    printf("total=%d x=%d sum=%d\n", total, x, sumArr(b, 6));
    int day = 3;
    switch (day + 1) { case 1: printf("one\n"); case 4: printf("four\n"); default: printf("fall\n"); }
    for (int i = 0; i < 3; i++) { for (int j = 0; j < 3; j++) { if (j == 1) break; printf("%d%d ", i, j); } }
    printf("\n");
}
