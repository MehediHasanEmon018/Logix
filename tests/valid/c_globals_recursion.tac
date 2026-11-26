    total = 0
    x = 10
    x_1 = 3
    param x_1
    call addTo, 1
    param 5
    call addTo, 1
    array b, 6, int
    param b
    param 6
    call fill, 2
    print "total="
    print total, "%d"
    print " x="
    print x_1, "%d"
    print " sum="
    param b
    param 6
    t1 = call sumArr, 2
    print t1, "%d"
    print "\n"
    day = 3
    t2 = day + 1
    t3 = t2
    t4 = t3 != 1
    ifFalse t4 goto L1
    t5 = t3 != 4
    ifFalse t5 goto L2
    goto L3
L1:
    print "one\n"
L2:
    print "four\n"
L3:
    print "fall\n"
L4:
    i = 0
L5:
    t6 = i < 3
    ifFalse t6 goto L6
    j = 0
L7:
    t7 = j < 3
    ifFalse t7 goto L8
    t8 = j == 1
    ifFalse t8 goto L9
    goto L8
L9:
    print i, "%d"
    print j, "%d"
    print " "
    t9 = j + 1
    j = t9
    goto L7
L8:
    t10 = i + 1
    i = t10
    goto L5
L6:
    print "\n"
func addTo, v
    t11 = v * 2
    x_2 = t11
    t12 = total + x_2
    total = t12
endfunc
func sumArr, a, n
    t13 = n == 0
    ifFalse t13 goto L10
    return 0
L10:
    t14 = n - 1
    t15 = a[t14]
    t16 = n - 1
    param a
    param t16
    t17 = call sumArr, 2
    t18 = t15 + t17
    return t18
endfunc
func fill, a_1, n_1
    i_1 = 0
L11:
    t19 = i_1 < n_1
    ifFalse t19 goto L12
    t20 = i_1 * i_1
    a_1[i_1] = t20
    t21 = i_1 + 1
    i_1 = t21
    goto L11
L12:
endfunc
