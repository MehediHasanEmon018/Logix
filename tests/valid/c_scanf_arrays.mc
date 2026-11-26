#include <stdio.h>

int add(int a, int b) {
    return a + b;
}

int main() {
    int n;
    printf("Enter n: ");
    scanf("%d", &n);

    int arr[100];
    int i;
    for (i = 0; i < n; i++) {
        scanf("%d", &arr[i]);
    }

    int max = arr[0], sum = 0;
    for (i = 0; i < n; i++) {
        if (arr[i] > max) max = arr[i];
        sum = add(sum, arr[i]);
    }
    printf("max = %d, sum = %d\n", max, sum);
    return 0;
}
