int main() {
    int s = 0;
    for (int i = 0; i < 10; i++) {
        if (i == 2) continue;
        if (i == 6) break;
        s += i;
    }
    int j = 0;
    do { j++; } while (j < 3);
    print s;
    print j;
}
