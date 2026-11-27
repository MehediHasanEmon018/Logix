public class Main {
    public static void main(String[] args) {
        int day = 3;
        switch (day) {
            case 1: System.out.println("Sat"); break;
            case 3: System.out.println("Mon");
            case 4: System.out.println("Tue"); break;
            default: System.out.println("Other");
        }
        int a = 7, b = 12;
        int big = a > b ? a : b;
        System.out.println("max = " + big + " " + Math.max(a, b) + " " + Math.min(a, b));
        System.out.println(Math.sqrt(16) + " " + Math.pow(2, 10) + " " + Math.abs(-5) + " " + Math.abs(-2.5));
        System.out.println(Math.PI * 2);
        char g = 'B';
        switch (g) { case 'A': System.out.println("top"); break; case 'B': System.out.println("good"); break; }
    }
}
