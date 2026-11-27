    total = 0
    x = 10
    x_2 = 3
    param x_2
    call addTo, 1
    param 5
    call addTo, 1
    array b 6
    paramarr b
    param 6
    call fill, 2
    print "total="
    print total, "%d"
    print " x="
    print x_2, "%d"
    print " sum="
    paramarr b
    param 6
    t12 = call sumArr, 2
    print t12, "%d"
    print "\n"
    day = 3
    t13 = day + 1
    t14 = t13
    t15 = t14 != 1
    ifFalse t15 goto L4
    t16 = t14 != 4
    ifFalse t16 goto L5
    goto L6
L4:
    print "one\n"
L5:
    print "four\n"
L6:
    print "fall\n"
L7:
    i_1 = 0
L8:
    t17 = i_1 < 3
    ifFalse t17 goto L9
    j = 0
L10:
    t18 = j < 3
    ifFalse t18 goto L11
    t19 = j == 1
    ifFalse t19 goto L12
    goto L11
L12:
    print i_1, "%d"
    print j, "%d"
    print " "
    t20 = j + 1
    j = t20
    goto L10
L11:
    t21 = i_1 + 1
    i_1 = t21
    goto L8
L9:
    print "\n"
    return
func addTo(v) locals(v, t1, x_1, t2)
    t1 = v * 2
    x_1 = t1
    t2 = total + x_1
    total = t2
endfunc
func sumArr(a[], n) locals(a[], n, t3, t4, t5, t6, t7, t8)
    t3 = n == 0
    ifFalse t3 goto L1
    return 0
L1:
    t4 = n - 1
    t5 = a[t4]
    t6 = n - 1
    paramarr a
    param t6
    t7 = call sumArr, 2
    t8 = t5 + t7
    return t8
endfunc
func fill(a_1[], n_1) locals(a_1[], n_1, i, t9, t10, t11)
    i = 0
L2:
    t9 = i < n_1
    ifFalse t9 goto L3
    t10 = i * i
    a_1[i] = t10
    t11 = i + 1
    i = t11
    goto L2
L3:
endfunc
