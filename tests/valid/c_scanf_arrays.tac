    n = 0
    print "Enter n: "
    n = read int
    array arr, 100, int
    i = 0
    i = 0
L1:
    t1 = i < n
    ifFalse t1 goto L2
    t2 = read int
    arr[i] = t2
    t3 = i + 1
    i = t3
    goto L1
L2:
    t4 = arr[0]
    max = t4
    sum = 0
    i = 0
L3:
    t5 = i < n
    ifFalse t5 goto L4
    t6 = arr[i]
    t7 = t6 > max
    ifFalse t7 goto L5
    t8 = arr[i]
    max = t8
L5:
    t9 = arr[i]
    param sum
    param t9
    t10 = call add, 2
    sum = t10
    t11 = i + 1
    i = t11
    goto L3
L4:
    print "max = "
    print max, "%d"
    print ", sum = "
    print sum, "%d"
    print "\n"
    return
func add, a, b
    t12 = a + b
    return t12
endfunc
