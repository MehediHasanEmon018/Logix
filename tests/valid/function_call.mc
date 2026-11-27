int square(int x) {
    return x * x;
}
int main() {
    int a[3] = {1, 2, 3};
    a[1] = square(a[2]);
    print a[1];
}
