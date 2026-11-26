#include <stdio.h>

int main() {
    int a = 10;
    int b = 3;
    float pi = 3.14;
    bool flag = true;

    int sum = a + b;
    int rem = a % b;
    float mix = sum + pi;

    a += 2;
    b--;

    printf("a = %d, b = %d, sum = %d, rem = %d\n", a, b, sum, rem);
    printf("mix = %.2f\n", mix);

    if (a > 10 && flag) {
        int x = 5;
        printf("Inside if: x = %d\n", x);
    } else {
        printf("Inside else\n");
    }

    int i = 0;
    while (i < 3) {
        printf("i = %d\n", i);
        i++;
    }

    int total = 0;
    for (int k = 1; k <= 4; k++) {
        total += k;
    }
    printf("total = %d\n", total);

    return 0;
}