public class Main {
    // method before main
    public static boolean isPrime(int n) {
        if (n < 2) return false;
        for (int i = 2; i * i <= n; i++) {
            if (n % i == 0) return false;
        }
        return true;
    }

    public static void main(String[] args) {
        int a = 15, b = 4;
        System.out.println("a + b = " + (a + b));
        System.out.println("a / b = " + a / b);
        System.out.println("a % b = " + a % b);
        System.out.println("a > b ? " + (a > b));
        int marks = 72;
        if (marks >= 80) System.out.println("A+");
        else if (marks >= 70) System.out.println("A");
        else if (marks >= 60) System.out.println("A-");
        else System.out.println("F");
        for (int i = 1; i <= 4; i++) {
            for (int j = 1; j <= i; j++) System.out.print(j + " ");
            System.out.println();
        }
        for (int p = 2; p < 20; p++) if (isPrime(p)) System.out.print(p + ",");
        System.out.println();
        double c = 9 / 2;
        double e = 9 / 2.0;
        System.out.println(c + " " + e);
        System.out.println(total);
        int x = 10;
        x *= 3; x -= 5; x++;
        System.out.println("x=" + x);
        /* block
           comment */
        System.out.println(-x + 1);
        System.out.println('A' + 1);
        System.out.println("" + 'A' + 1);
    }
    static int total = 42;
}
