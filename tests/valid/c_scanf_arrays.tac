    print "Enter n: "
    scan n, d
    array arr 100
    i = 0
L1:
    t2 = i < n
    ifFalse t2 goto L2
    scan t3, d
    arr[i] = t3
    t4 = i + 1
    i = t4
    goto L1
L2:
    t5 = arr[0]
    max = t5
    sum = 0
    i = 0
L3:
    t6 = i < n
    ifFalse t6 goto L4
    t7 = arr[i]
    t8 = t7 > max
    ifFalse t8 goto L5
    t9 = arr[i]
    max = t9
L5:
    t10 = arr[i]
    param sum
    param t10
    t11 = call add, 2
    sum = t11
    t12 = i + 1
    i = t12
    goto L3
L4:
    print "max = "
    print max, "%d"
    print ", sum = "
    print sum, "%d"
    print "\n"
    return 0
    return
func add(a, b) locals(a, b, t1)
    t1 = a + b
    return t1
endfunc
