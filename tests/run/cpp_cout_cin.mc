#include <iostream>
using namespace std;

int square(int x) {
    return x * x;
}

int main() {
    int n;
    cout << "Koyta number? ";
    cin >> n;

    int nums[100];
    int sum = 0;
    for (int i = 0; i < n; i++) {
        cin >> nums[i];
        sum += nums[i];
        cout << nums[i] << " er square = " << square(nums[i]) << endl;
    }

    cout << "Total sum = " << sum << endl;
    return 0;
}
