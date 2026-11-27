    array a 3
    a[0] = 1
    a[1] = 2
    a[2] = 3
    t2 = a[2]
    param t2
    t3 = call square, 1
    a[1] = t3
    t4 = a[1]
    print t4
    return
func square(x) locals(x, t1)
    t1 = x * x
    return t1
endfunc
