    t1 = 7 % 3
    a = t1
    f = 3.14
    t2 = a >= 1
    t3 = t2
    ifFalse t3 goto L1
    t4 = f < 1.0
    t5 = ! t4
    t3 = t5
L1:
    t6 = t3
    ifFalse t6 goto L3
    goto L2
L3:
    t6 = false
L2:
    ok = t6
    ifFalse ok goto L4
    t7 = a * 2
    t8 = t7 + 1
    inner = t8
L5:
    t9 = inner != 0
    ifFalse t9 goto L6
    t10 = inner - 1
    inner = t10
    t11 = inner == 2
    ifFalse t11 goto L7
    print inner
L7:
    goto L5
L6:
L4:
