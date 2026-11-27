import java.util.Scanner;

public class Main {
    static int count = 0;
    static final int LIMIT = 10;

    public static void main(String[] args) {
        Scanner sc = new Scanner(System.in);
        int n = sc.nextInt();
        int[] arr = new int[5];
        for (int i = 0; i < n; i++) {
            arr[i] = sc.nextInt();
        }
        double d = sc.nextDouble();
        char op = sc.next().charAt(0);

        int sum = 0;
        for (int x : arr) sum += x;
        System.out.println("Sum = " + sum);
        System.out.println("Max = " + max(arr, n));
        System.out.println("Average: " + (double) sum / n);
        System.out.println(sum + n + " total");
        System.out.println("d = " + d + ", op = " + op);
        System.out.print("Length " + arr.length);
        System.out.println();

        if (sum % 2 == 0) {
            System.out.println("Even");
        } else if (sum > 100) {
            System.out.println("Big odd");
        } else {
            System.out.println("Odd");
        }

        boolean found = false;
        int i = 0;
        while (i < LIMIT) {
            i++;
            if (i == 3) continue;
            if (i == 7) { found = true; break; }
            count++;
        }
        System.out.println("found = " + found + " count = " + count);

        int k = 0;
        do { k += 2; } while (k < 9);
        System.out.println(k);
        System.out.println(fact(5));
        System.out.println(10 / 4.0);
        System.out.println(10.0);
        System.out.println(1.0 / 3);
        System.out.printf("%.2f%n", 3.14159);
        System.out.printf("%d items%n", n);
        char grade = 'A';
        System.out.println("Grade: " + grade);
        System.out.println(grade == 'A');
        int[] b = {3, 1, 2};
        System.out.println(b[0] + b[1] + b[2]);
        long big = 100000L;
        System.out.println((int) 7.9 + " " + big);
        printLine(3);
        sc.close();
    }

    static int max(int[] a, int n) {
        int m = a[0];
        for (int i = 1; i < n; i++) if (a[i] > m) m = a[i];
        return m;
    }

    static int fact(int n) {
        if (n <= 1) return 1;
        return n * fact(n - 1);
    }

    static void printLine(int n) {
        for (int i = 0; i < n; i++) System.out.print("*");
        System.out.println();
    }
}
