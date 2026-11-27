    count = 0
    array a_3 5
    a_3[0] = 5
    a_3[1] = 3
    a_3[2] = 9
    a_3[3] = 1
    a_3[4] = 7
    big = 100000
    k = 3
    grade = 66
    paramarr a_3
    param 5
    call printArr, 2
    paramarr a_3
    param 5
    call bubble, 2
    paramarr a_3
    param 5
    call printArr, 2
    print "avg = "
    paramarr a_3
    param 5
    t40 = call avg, 2
    print t40, "%.2f"
    print "\n"
    print "5! = "
    param 5
    t41 = call fact, 1
    print t41, "%d"
    print ", fib(10) = "
    param 10
    t42 = call fib, 1
    print t42, "%d"
    print "\n"
    print "sqrt(16)="
    param 16
    t43 = call sqrt, 1
    print t43, "%.1f"
    print " pow(2,10)="
    param 2
    param 10
    t44 = call pow, 2
    print t44, "%.0f"
    print " abs(-4)="
    param -4
    t45 = call abs, 1
    print t45, "%d"
    print " area="
    t46 = (float) k
    t47 = 3.14159 * t46
    t48 = (float) k
    t49 = t47 * t48
    print t49, "%.3f"
    print "\n"
    i_3 = 0
L13:
    t50 = i_3 + 1
    i_3 = t50
    t51 = i_3 == 2
    ifFalse t51 goto L16
    goto L15
L16:
    t52 = i_3 > 6
    ifFalse t52 goto L17
    goto L14
L17:
    print i_3, "%d"
    print ","
L15:
    t53 = i_3 < 10
    ifFalse t53 goto L14
    goto L13
L14:
    print "\n"
    x = 1
L18:
    t54 = x <= 20
    ifFalse t54 goto L19
    t55 = x % 2
    t56 = t55 == 0
    ifFalse t56 goto L21
    goto L20
L21:
    t57 = x > 9
    ifFalse t57 goto L22
    goto L19
L22:
    t58 = count + 1
    count = t58
L20:
    t59 = x + 1
    x = t59
    goto L18
L19:
    print "odd count = "
    print count, "%d"
    print "\n"
    t60 = grade != 65
    ifFalse t60 goto L23
    t61 = grade != 66
    ifFalse t61 goto L24
    t62 = grade != 67
    ifFalse t62 goto L25
    goto L26
L23:
    print "Excellent\n"
    goto L27
L24:
L25:
    print "Good "
    print grade, "%c"
    print "\n"
    goto L27
L26:
    print "??\n"
L27:
    marks = 72
    t63 = marks >= 80
    ifFalse t63 goto L28
    print "A+\n"
    goto L29
L28:
    t64 = marks >= 70
    ifFalse t64 goto L30
    print "A\n"
    goto L31
L30:
    print "F\n"
L31:
L29:
    p = 2
L32:
    t65 = p <= 20
    ifFalse t65 goto L33
    param p
    t66 = call isPrime, 1
    t67 = t66 == 1
    ifFalse t67 goto L34
    print p, "%d"
    print " "
L34:
    t68 = p + 1
    p = t68
    goto L32
L33:
    print "\nbig="
    print big, "%d"
    print " 100%\n"
    array m 3
    m[0] = 1
    t69 = m[1]
    t70 = t69 + 5
    m[1] = t70
    t71 = m[2]
    t72 = t71 + 1
    m[2] = t72
    t73 = m[0]
    print t73, "%d"
    print " "
    t74 = m[1]
    print t74, "%d"
    print " "
    t75 = m[2]
    print t75, "%d"
    print "\n"
    return 0
    print "never\n"
    return
func fact(n) locals(n, t1, t2, t3, t4)
    t1 = n <= 1
    ifFalse t1 goto L1
    return 1
L1:
    t2 = n - 1
    param t2
    t3 = call fact, 1
    t4 = n * t3
    return t4
endfunc
func fib(n_1) locals(n_1, t5, t6, t7, t8, t9, t10, t11)
    t5 = n_1 < 2
    ifFalse t5 goto L2
    t6 = n_1
    goto L3
L2:
    t7 = n_1 - 1
    param t7
    t8 = call fib, 1
    t9 = n_1 - 2
    param t9
    t10 = call fib, 1
    t11 = t8 + t10
    t6 = t11
L3:
    return t6
endfunc
func printArr(a[], n_2) locals(a[], n_2, i, t12, t13, t14)
    i = 0
L4:
    t12 = i < n_2
    ifFalse t12 goto L5
    t13 = a[i]
    print t13, "%d"
    print " "
    t14 = i + 1
    i = t14
    goto L4
L5:
    print "\n"
endfunc
func bubble(a_1[], n_3) locals(a_1[], n_3, i_1, t15, t16, j, t17, t18, t19, t20, t21, t22, t23, t24, t, t25, t26, t27, t28, t29)
    i_1 = 0
L6:
    t15 = n_3 - 1
    t16 = i_1 < t15
    ifFalse t16 goto L7
    j = 0
L8:
    t17 = n_3 - i_1
    t18 = t17 - 1
    t19 = j < t18
    ifFalse t19 goto L9
    t20 = a_1[j]
    t21 = j + 1
    t22 = a_1[t21]
    t23 = t20 > t22
    ifFalse t23 goto L10
    t24 = a_1[j]
    t = t24
    t25 = j + 1
    t26 = a_1[t25]
    a_1[j] = t26
    t27 = j + 1
    a_1[t27] = t
L10:
    t28 = j + 1
    j = t28
    goto L8
L9:
    t29 = i_1 + 1
    i_1 = t29
    goto L6
L7:
endfunc
func avg(a_2[], n_4) locals(a_2[], n_4, s, i_2, t30, t31, t32, t33, t34, t35, t36, t37, t38, t39)
    s = 0
    i_2 = 0
L11:
    t30 = i_2 < n_4
    ifFalse t30 goto L12
    t31 = a_2[i_2]
    t32 = s + t31
    s = t32
    t33 = i_2 + 1
    i_2 = t33
    goto L11
L12:
    t34 = (float) 0
    t35 = (float) n_4
    t36 = t35 * 1.0
    t37 = (float) s
    t38 = t37 / t36
    t39 = t34 + t38
    return t39
endfunc
func isPrime(n_5) locals(n_5, t76, i_4, t77, t78, t79, t80, t81)
    t76 = n_5 < 2
    ifFalse t76 goto L35
    return 0
L35:
    i_4 = 2
L36:
    t77 = i_4 * i_4
    t78 = t77 <= n_5
    ifFalse t78 goto L37
    t79 = n_5 % i_4
    t80 = t79 == 0
    ifFalse t80 goto L38
    return 0
L38:
    t81 = i_4 + 1
    i_4 = t81
    goto L36
L37:
    return 1
endfunc
