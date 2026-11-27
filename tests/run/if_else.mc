#include <stdio.h>
int main() {
    int a, b;
    printf("Duita number dao: ");
    scanf("%d %d", &a, &b);
    printf("Sum = %d\n", a + b);
    if (a > b) {
        printf("a boro\n");
    } else if (a < b) {
        printf("b boro\n");
    } else {
        printf("soman\n");
    }
    return 0;
}
