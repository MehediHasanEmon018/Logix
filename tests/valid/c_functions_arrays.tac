    count = 0
    array a, 5, int
    a[0] = 5
    a[1] = 3
    a[2] = 9
    a[3] = 1
    a[4] = 7
    big = 100000
    k = 3
    grade = 66
    param a
    param 5
    call printArr, 2
    param a
    param 5
    call bubble, 2
    param a
    param 5
    call printArr, 2
    print "avg = "
    param a
    param 5
    t1 = call avg, 2
    print t1, "%.2f"
    print "\n"
    print "5! = "
    param 5
    t2 = call fact, 1
    print t2, "%d"
    print ", fib(10) = "
    param 10
    t3 = call fib, 1
    print t3, "%d"
    print "\n"
    print "sqrt(16)="
    t4 = (float) 16
    param t4
    t5 = call sqrt, 1
    print t5, "%.1f"
    print " pow(2,10)="
    t6 = (float) 2
    t7 = (float) 10
    param t6
    param t7
    t8 = call pow, 2
    print t8, "%.0f"
    print " abs(-4)="
    param -4
    t9 = call abs, 1
    print t9, "%d"
    print " area="
    t10 = (float) k
    t11 = 3.14159 * t10
    t12 = (float) k
    t13 = t11 * t12
    print t13, "%.3f"
    print "\n"
    i = 0
L1:
    t14 = i + 1
    i = t14
    t15 = i == 2
    ifFalse t15 goto L2
    goto L3
L2:
    t16 = i > 6
    ifFalse t16 goto L4
    goto L5
L4:
    print i, "%d"
    print ","
L3:
    t17 = i < 10
    ifFalse t17 goto L5
    goto L1
L5:
    print "\n"
    x = 1
L6:
    t18 = x <= 20
    ifFalse t18 goto L7
    t19 = x % 2
    t20 = t19 == 0
    ifFalse t20 goto L8
    goto L9
L8:
    t21 = x > 9
    ifFalse t21 goto L10
    goto L7
L10:
    t22 = count + 1
    count = t22
L9:
    t23 = x + 1
    x = t23
    goto L6
L7:
    print "odd count = "
    print count, "%d"
    print "\n"
    t24 = grade != 65
    ifFalse t24 goto L11
    t25 = grade != 66
    ifFalse t25 goto L12
    t26 = grade != 67
    ifFalse t26 goto L13
    goto L14
L11:
    print "Excellent\n"
    goto L15
L12:
L13:
    print "Good "
    print grade, "%c"
    print "\n"
    goto L15
L14:
    print "??\n"
L15:
    marks = 72
    t27 = marks >= 80
    ifFalse t27 goto L16
    print "A+\n"
    goto L17
L16:
    t28 = marks >= 70
    ifFalse t28 goto L18
    print "A\n"
    goto L19
L18:
    print "F\n"
L19:
L17:
    p = 2
L20:
    t29 = p <= 20
    ifFalse t29 goto L21
    param p
    t30 = call isPrime, 1
    t31 = t30 == 1
    ifFalse t31 goto L22
    print p, "%d"
    print " "
L22:
    t32 = p + 1
    p = t32
    goto L20
L21:
    print "\nbig="
    print big, "%d"
    print " 100%\n"
    array m, 3, int
    m[0] = 1
    t33 = m[1]
    t34 = t33 + 5
    m[1] = t34
    t35 = m[2]
    t36 = t35 + 1
    m[2] = t36
    t37 = m[0]
    print t37, "%d"
    print " "
    t38 = m[1]
    print t38, "%d"
    print " "
    t39 = m[2]
    print t39, "%d"
    print "\n"
    return
    print "never\n"
func fact, n
    t40 = n <= 1
    ifFalse t40 goto L23
    return 1
L23:
    t41 = n - 1
    param t41
    t42 = call fact, 1
    t43 = n * t42
    return t43
endfunc
func fib, n_1
    t44 = n_1 < 2
    ifFalse t44 goto L24
    t45 = n_1
    goto L25
L24:
    t46 = n_1 - 1
    param t46
    t47 = call fib, 1
    t48 = n_1 - 2
    param t48
    t49 = call fib, 1
    t50 = t47 + t49
    t45 = t50
L25:
    return t45
endfunc
func printArr, a_1, n_2
    i_1 = 0
L26:
    t51 = i_1 < n_2
    ifFalse t51 goto L27
    t52 = a_1[i_1]
    print t52, "%d"
    print " "
    t53 = i_1 + 1
    i_1 = t53
    goto L26
L27:
    print "\n"
endfunc
func bubble, a_2, n_3
    i_2 = 0
L28:
    t54 = n_3 - 1
    t55 = i_2 < t54
    ifFalse t55 goto L29
    j = 0
L30:
    t56 = n_3 - i_2
    t57 = t56 - 1
    t58 = j < t57
    ifFalse t58 goto L31
    t59 = a_2[j]
    t60 = j + 1
    t61 = a_2[t60]
    t62 = t59 > t61
    ifFalse t62 goto L32
    t63 = a_2[j]
    t = t63
    t64 = j + 1
    t65 = a_2[t64]
    a_2[j] = t65
    t66 = j + 1
    a_2[t66] = t
L32:
    t67 = j + 1
    j = t67
    goto L30
L31:
    t68 = i_2 + 1
    i_2 = t68
    goto L28
L29:
endfunc
func avg, a_3, n_4
    s = 0
    i_3 = 0
L33:
    t69 = i_3 < n_4
    ifFalse t69 goto L34
    t70 = a_3[i_3]
    t71 = s + t70
    s = t71
    t72 = i_3 + 1
    i_3 = t72
    goto L33
L34:
    t73 = (float) 0
    t74 = (float) n_4
    t75 = t74 * 1.0
    t76 = (float) s
    t77 = t76 / t75
    t78 = t73 + t77
    return t78
endfunc
func isPrime, n_5
    t79 = n_5 < 2
    ifFalse t79 goto L35
    return 0
L35:
    i_4 = 2
L36:
    t80 = i_4 * i_4
    t81 = t80 <= n_5
    ifFalse t81 goto L37
    t82 = n_5 % i_4
    t83 = t82 == 0
    ifFalse t83 goto L38
    return 0
L38:
    t84 = i_4 + 1
    i_4 = t84
    goto L36
L37:
    return 1
endfunc
